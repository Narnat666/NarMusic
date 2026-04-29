#include "core/logger.h"
#include "config/config.h"
#include "core/epoll_server.h"
#include "core/http/router.h"
#include "core/http/request.h"
#include "core/http/response.h"
#include "core/rate_limiter.h"

#include "infrastructure/persistence/database.h"
#include "infrastructure/persistence/sqlite_task_repository.h"
#include "infrastructure/persistence/sqlite_music_library_repository.h"
#include "infrastructure/http_client/curl_client.h"
#include "infrastructure/bilibili/bilibili_client.h"
#include "infrastructure/lyrics/lyrics_aggregator.h"
#include "infrastructure/lyrics/kugou_provider.h"
#include "infrastructure/lyrics/netease_provider.h"
#include "infrastructure/lyrics/qqmusic_provider.h"
#include "infrastructure/lyrics/qishui_provider.h"
#include "infrastructure/audio/audio_downloader.h"
#include "infrastructure/streaming/stream_sender.h"
#include "infrastructure/filesystem/music_file_repository.h"
#include "infrastructure/tunnel/cpolar_tunnel.h"

#include "application/download_service.h"
#include "application/search_service.h"
#include "application/library_service.h"
#include "application/streaming_service.h"

#include "presentation/controllers/download_controller.h"
#include "presentation/controllers/search_controller.h"
#include "presentation/controllers/library_controller.h"
#include "presentation/middleware/static_file_handler.h"

#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <curl/curl.h>
#include <filesystem>
#include <taglib/mp4/mp4file.h>
#include <taglib/mp4/mp4tag.h>
#include "version.h"

using namespace narnat;

static int analyzePlaylist(const std::string& dirPath) {
    namespace fs = std::filesystem;
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "目录不存在: " << dirPath << std::endl;
        return 1;
    }

    int count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".m4a") continue;

        TagLib::MP4::File file(entry.path().c_str());
        if (!file.isValid()) continue;
        auto* tag = file.tag();
        if (!tag) continue;

        if (tag->contains("----:com.narnat:narmeta")) {
            auto item = tag->item("----:com.narnat:narmeta");
            std::string val = item.toStringList().toString().to8Bit(true);
            if (!val.empty()) {
                std::cout << val << std::endl;
                count++;
            }
        }
    }

    if (count == 0) {
        std::cerr << "未找到包含narmeta信息的m4a文件" << std::endl;
    }
    return 0;
}

static EpollServer* gServer = nullptr;

void signalHandler(int) {
    if (gServer) gServer->stop();
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        std::cerr << "curl全局初始化失败" << std::endl;
        return 1;
    }
    atexit([]() { curl_global_cleanup(); });

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--version") {
            std::cout << NARNAT_VERSION << " (" << NARNAT_TARGET_ARCH << ")" << std::endl;
            return 0;
        }
        if (arg == "-a" && i + 1 < argc) {
            return analyzePlaylist(argv[i + 1]);
        }
    }

    int port = 0;
    std::string downloadPath, extension, cpolarToken, emailKey;
    bool debug = false;
    std::string configPath = "./config.json";

    int opt;
    while ((opt = getopt(argc, argv, "o:p:e:c:t:m:dh")) != -1) {
        switch (opt) {
            case 'p': downloadPath = optarg; break;
            case 'e': extension = optarg; break;
            case 'o': port = std::stoi(optarg); break;
            case 'c': configPath = optarg; break;
            case 't': cpolarToken = optarg; break;
            case 'm': emailKey = optarg; break;
            case 'd': debug = true; break;
            case 'h':
                std::cout << "用法: NarMusic [-p path] [-e ext] [-o port] [-c config] [-t token] [-m key] [-d] [-a dir]" << std::endl;
                std::cout << "  -p path  音频文件保存目录" << std::endl;
                std::cout << "  -e ext   音频文件扩展名" << std::endl;
                std::cout << "  -o port  HTTP服务端口" << std::endl;
                std::cout << "  -c file  配置文件路径" << std::endl;
                std::cout << "  -t token cpolar authtoken (启动内网穿透)" << std::endl;
                std::cout << "  -m key   邮箱授权码 (邮箱:授权码 格式启用邮件通知)" << std::endl;
                std::cout << "  -d       调试模式" << std::endl;
                std::cout << "  -a dir   解析目录下m4a文件的narmeta信息并输出歌单" << std::endl;
                std::cout << "  --version   输出版本信息" << std::endl;
                return 0;
        }
    }

    namespace fs = std::filesystem;
    if (!fs::exists(configPath)) {
        try {
            auto exePath = fs::canonical("/proc/self/exe");
            auto etcDir = exePath.parent_path().parent_path() / "etc" / "NarMusic" / "config.json";
            if (fs::exists(etcDir)) configPath = etcDir.string();
        } catch (...) {}
    }
    Config config = Config::load(configPath);
    config.applyOverrides(port, downloadPath, extension, debug, cpolarToken, emailKey);

    Logger::Config logCfg;
    logCfg.level = debug ? Logger::Level::DEBUG : Logger::Level::INFO;
    if (config.logging.level == "debug") logCfg.level = Logger::Level::DEBUG;
    else if (config.logging.level == "warn") logCfg.level = Logger::Level::WARN;
    else if (config.logging.level == "error") logCfg.level = Logger::Level::ERROR;
    logCfg.file_path = config.logging.file;
    logCfg.max_size_mb = config.logging.max_size_mb;
    logCfg.console_output = true;
    Logger::instance().init(logCfg);

    LOG_I("Main", "NarMusic 启动中...");

    fs::create_directories(config.download.path);

    // ===== 依赖注入 =====

    auto db = std::make_shared<Database>(config.database.path);
    auto taskRepo = std::make_shared<SqliteTaskRepository>(db);
    auto libraryRepo = std::make_shared<SqliteMusicLibraryRepository>(db);
    auto fileRepo = std::make_shared<FsMusicFileRepository>();

    CurlClient::Options curlOpts;
    curlOpts.connectTimeout = config.bilibili.connect_timeout;
    curlOpts.requestTimeout = config.bilibili.request_timeout;
    curlOpts.userAgent = config.bilibili.user_agent;
    auto curlClient = std::make_shared<CurlClient>(curlOpts);

    auto biliClient = std::make_shared<BilibiliClient>(curlClient);
    auto audioDownloader = std::make_shared<AudioDownloader>(biliClient);

    auto lyricsAggregator = std::make_shared<LyricsAggregator>();
    lyricsAggregator->addProvider(std::make_shared<KugouProvider>(curlClient));
    lyricsAggregator->addProvider(std::make_shared<NeteaseProvider>(curlClient));
    lyricsAggregator->addProvider(std::make_shared<QQMusicProvider>(curlClient));
    lyricsAggregator->addProvider(std::make_shared<QishuiProvider>(curlClient));

    auto downloadService = std::make_shared<DownloadService>(
        taskRepo, libraryRepo, fileRepo, audioDownloader, lyricsAggregator, config.download);
    auto searchService = std::make_shared<SearchService>(biliClient);
    auto libraryService = std::make_shared<LibraryService>(libraryRepo);
    auto streamingService = std::make_shared<StreamingService>(taskRepo, libraryRepo, config.download);

    auto downloadCtrl = std::make_shared<DownloadController>(downloadService, streamingService);
    auto searchCtrl = std::make_shared<SearchController>(searchService);
    auto libraryCtrl = std::make_shared<LibraryController>(libraryService);

    std::string webDir = "./web";
    if (!fs::exists(webDir + "/index.html")) {
        try {
            auto exePath = fs::canonical("/proc/self/exe");
            auto shareDir = exePath.parent_path().parent_path() / "share" / "NarMusic" / "web";
            if (fs::exists(shareDir / "index.html")) webDir = shareDir.string();
        } catch (...) {}
    }
    LOG_I("Main", "Web目录: " + webDir);
    auto staticHandler = std::make_shared<StaticFileHandler>(webDir);

    // ===== 路由注册 =====
    Router router;

    auto rateLimiter = std::make_shared<RateLimiter>(RateLimiter::Config{60, 60});
    router.addMiddleware([rateLimiter](const Request& req) -> std::optional<Response> {
        if (!rateLimiter->allow(req.path())) {
            return Response::error(429, "Too Many Requests", "rate_limited",
                                   "请求过于频繁，请稍后再试");
        }
        return std::nullopt;
    });

    router.addRoute(Request::Method::POST, "/api/message",
        [downloadCtrl](const Request& req) { return downloadCtrl->createTask(req); });

    router.addRoute(Request::Method::GET, "/api/download/status",
        [downloadCtrl](const Request& req) { return downloadCtrl->getStatus(req); });

    router.addRoute(Request::Method::POST, "/api/download/status/batch",
        [downloadCtrl](const Request& req) { return downloadCtrl->batchGetStatus(req); });

    router.addRoute(Request::Method::GET, "/api/download/file",
        [downloadCtrl](const Request& req) { return downloadCtrl->downloadFile(req); });

    router.addRoute(Request::Method::GET, "/api/download/stream",
        [downloadCtrl](const Request& req) { return downloadCtrl->stream(req); });

    router.addRoute(Request::Method::GET, "/api/search",
        [searchCtrl](const Request& req) { return searchCtrl->search(req); });

    router.addRoute(Request::Method::POST, "/api/search/batch",
        [searchCtrl](const Request& req) { return searchCtrl->batchSearch(req); });

    router.addRoute(Request::Method::POST, "/api/download/batch",
        [downloadCtrl](const Request& req) { return downloadCtrl->batchCreateTasks(req); });

    router.addRoute(Request::Method::GET, "/api/library/list",
        [libraryCtrl](const Request& req) { return libraryCtrl->list(req); });

    router.addRoute(Request::Method::DELETE, "/api/library/delete",
        [libraryCtrl](const Request& req) { return libraryCtrl->remove(req); });

    router.addRoute(Request::Method::POST, "/api/library/batch-delete",
        [libraryCtrl](const Request& req) { return libraryCtrl->batchRemove(req); });

    router.addRoute(Request::Method::POST, "/api/library/batch-download",
        [libraryCtrl](const Request& req) { return libraryCtrl->batchDownload(req); });

    router.addCatchAllRoute(Request::Method::GET,
        [staticHandler](const Request& req) { return staticHandler->handle(req); });

    // ===== 启动服务器 =====
    EpollServer server(config.server, router);
    gServer = &server;

    // ===== cpolar 内网穿透 (服务器启动后延迟启动) =====
    std::unique_ptr<CpolarTunnel> tunnel;
    std::thread cpolarThread;
    if (config.cpolar.enabled && !config.cpolar.authtoken.empty()) {
        cpolarThread = std::thread([&]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            tunnel = std::make_unique<CpolarTunnel>(config.cpolar, config.email, config.server.port);
            if (!tunnel->start()) {
                LOG_W("Main", "cpolar 内网穿透启动失败，服务器仍可本地访问");
                tunnel.reset();
            }
        });
    }

    std::atomic<bool> running{true};
    std::thread cleanupThread([&]() {
        while (running) {
            for (int i = 0; i < config.download.cleanup_interval && running; ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            if (running) {
                downloadService->cleanupExpiredTasks();
                rateLimiter->cleanup();
            }
        }
    });

    server.start();

    running = false;
    cleanupThread.join();
    gServer = nullptr;

    if (tunnel) {
        tunnel->stop();
        tunnel.reset();
    }
    if (cpolarThread.joinable()) {
        cpolarThread.join();
    }

    LOG_I("Main", "NarMusic 已停止");
    Logger::instance().shutdown();
    return 0;
}
