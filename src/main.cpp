#include "core/logger.h"
#include "config/config.h"
#include "core/epoll_server.h"
#include "core/http/router.h"
#include "core/http/request.h"
#include "core/http/response.h"

// Infrastructure
#include "infrastructure/persistence/database.h"
#include "infrastructure/persistence/sqlite_task_repository.h"
#include "infrastructure/http_client/curl_client.h"
#include "infrastructure/bilibili/bilibili_client.h"
#include "infrastructure/lyrics/lyrics_aggregator.h"
#include "infrastructure/lyrics/kugou_provider.h"
#include "infrastructure/lyrics/netease_provider.h"
#include "infrastructure/lyrics/qqmusic_provider.h"
#include "infrastructure/audio/audio_downloader.h"
#include "infrastructure/streaming/stream_sender.h"
#include "infrastructure/filesystem/music_file_repository.h"

// Application
#include "application/download_service.h"
#include "application/search_service.h"
#include "application/library_service.h"
#include "application/streaming_service.h"

// Presentation
#include "presentation/controllers/download_controller.h"
#include "presentation/controllers/search_controller.h"
#include "presentation/controllers/library_controller.h"
#include "presentation/middleware/static_file_handler.h"

#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <curl/curl.h>
#include <filesystem>

using namespace narnat;

static EpollServer* gServer = nullptr;

void signalHandler(int) {
    if (gServer) gServer->stop();
}

int main(int argc, char* argv[]) {
    // 忽略SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 初始化curl
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        std::cerr << "curl全局初始化失败" << std::endl;
        return 1;
    }
    atexit([]() { curl_global_cleanup(); });

    // 解析命令行参数
    int port = 0;
    std::string downloadPath, extension;
    bool debug = false;
    std::string configPath = "./config.json";

    int opt;
    while ((opt = getopt(argc, argv, "o:p:e:c:dh")) != -1) {
        switch (opt) {
            case 'p': downloadPath = optarg; break;
            case 'e': extension = optarg; break;
            case 'o': port = std::stoi(optarg); break;
            case 'c': configPath = optarg; break;
            case 'd': debug = true; break;
            case 'h':
                std::cout << "用法: NarMusic [-p path] [-e ext] [-o port] [-c config] [-d]" << std::endl;
                return 0;
        }
    }

    // 加载配置
    Config config = Config::load(configPath);
    config.applyOverrides(port, downloadPath, extension, debug);

    // 初始化日志
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

    // 清理旧文件
    namespace fs = std::filesystem;
    if (fs::exists(config.download.path) && fs::is_directory(config.download.path)) {
        int count = 0;
        for (const auto& entry : fs::directory_iterator(config.download.path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".m4a") {
                fs::remove(entry.path());
                count++;
            }
        }
        if (count > 0) LOG_I("Main", "已清理 " + std::to_string(count) + " 个旧文件");
    }
    fs::create_directories(config.download.path);

    // ===== 依赖注入 =====

    // Infrastructure层
    auto db = std::make_shared<Database>(config.database.path);
    auto taskRepo = std::make_shared<SqliteTaskRepository>(db);
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

    // Application层
    auto downloadService = std::make_shared<DownloadService>(
        taskRepo, fileRepo, audioDownloader, lyricsAggregator, config.download);
    auto searchService = std::make_shared<SearchService>(biliClient);
    auto libraryService = std::make_shared<LibraryService>(fileRepo, config.download);
    auto streamingService = std::make_shared<StreamingService>(taskRepo, config.download);

    // Presentation层
    auto downloadCtrl = std::make_shared<DownloadController>(downloadService, streamingService);
    auto searchCtrl = std::make_shared<SearchController>(searchService);
    auto libraryCtrl = std::make_shared<LibraryController>(libraryService);
    auto staticHandler = std::make_shared<StaticFileHandler>("./web");

    // ===== 路由注册 =====
    Router router;

    // API路由
    router.addRoute(Request::Method::POST, "/api/message",
        [downloadCtrl](const Request& req) { return downloadCtrl->createTask(req); });

    router.addRoute(Request::Method::GET, "/api/download/status",
        [downloadCtrl](const Request& req) { return downloadCtrl->getStatus(req); });

    router.addRoute(Request::Method::GET, "/api/download/file",
        [downloadCtrl](const Request& req) { return downloadCtrl->downloadFile(req); });

    router.addRoute(Request::Method::GET, "/api/download/stream",
        [downloadCtrl](const Request& req) { return downloadCtrl->stream(req); });

    router.addRoute(Request::Method::GET, "/api/search",
        [searchCtrl](const Request& req) { return searchCtrl->search(req); });

    router.addRoute(Request::Method::GET, "/api/library/list",
        [libraryCtrl](const Request& req) { return libraryCtrl->list(req); });

    // 静态文件兜底路由（所有GET请求）
    router.addCatchAllRoute(Request::Method::GET,
        [staticHandler](const Request& req) { return staticHandler->handle(req); });

    // ===== 启动服务器 =====
    EpollServer server(config.server, router);
    gServer = &server;

    // 启动定时清理线程
    std::atomic<bool> running{true};
    std::thread cleanupThread([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(config.download.cleanup_interval));
            downloadService->cleanupExpiredTasks();
        }
    });

    server.start();

    running = false;
    cleanupThread.join();
    gServer = nullptr;

    LOG_I("Main", "NarMusic 已停止");
    return 0;
}
