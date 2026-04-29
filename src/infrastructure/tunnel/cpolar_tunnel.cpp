#include "cpolar_tunnel.h"
#include "core/logger.h"
#include "infrastructure/http_client/curl_client.h"
#include "infrastructure/notification/email_sender.h"
#include "nlohmann/json.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <chrono>
#include <thread>
#include <filesystem>
#include <climits>
#include <algorithm>

namespace narnat {

namespace fs = std::filesystem;

static constexpr int CPOLAR_API_PORT = 4040;

std::string CpolarTunnel::cleanUrl(const std::string& url) {
    auto start = url.find("http");
    if (start == std::string::npos) return {};

    std::string cleaned;
    for (size_t i = start; i < url.size(); ++i) {
        char c = url[i];
        if (c == '`' || c == '\\' || c == '"' || c == '\'' ||
            c == '\n' || c == '\r' || c == ' ' || c == '>' || c == '<')
            break;
        cleaned += c;
    }
    while (!cleaned.empty() && cleaned.back() == '/')
        cleaned.pop_back();
    return cleaned;
}

std::string CpolarTunnel::resolveExeDir() {
    char path[PATH_MAX] = {};
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        return fs::path(path).parent_path().string();
    }
    return ".";
}

bool CpolarTunnel::resolveBinary() {
    if (!cpolarConfig_.bin_path.empty() && cpolarConfig_.bin_path != "cpolar") {
        resolvedBinPath_ = cpolarConfig_.bin_path;
        if (fs::exists(resolvedBinPath_)) {
            LOG_I("CpolarTunnel", "使用配置的 cpolar: " + resolvedBinPath_);
            return true;
        }
        LOG_W("CpolarTunnel", "配置的 cpolar 路径不存在: " + resolvedBinPath_);
    }

    std::string exeDir = resolveExeDir();
    std::vector<std::string> candidates = {
        exeDir + "/cpolar",
        exeDir + "/bin/cpolar",
        exeDir + "/../bin/cpolar",
        "/usr/local/bin/cpolar",
        "/usr/bin/cpolar",
        "cpolar"
    };

    for (const auto& candidate : candidates) {
        if (candidate == "cpolar") {
            std::string cmd = "which cpolar 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buf[PATH_MAX];
                if (fgets(buf, sizeof(buf), pipe)) {
                    pclose(pipe);
                    std::string found(buf);
                    while (!found.empty() && (found.back() == '\n' || found.back() == '\r'))
                        found.pop_back();
                    resolvedBinPath_ = found;
                    LOG_I("CpolarTunnel", "在 PATH 中找到 cpolar: " + resolvedBinPath_);
                    return true;
                }
                pclose(pipe);
            }
            continue;
        }

        if (fs::exists(candidate)) {
            resolvedBinPath_ = candidate;
            LOG_I("CpolarTunnel", "找到内嵌 cpolar: " + resolvedBinPath_);
            return true;
        }
    }

    LOG_E("CpolarTunnel", "未找到 cpolar 二进制文件，请安装或将 cpolar 放到程序同目录下");
    LOG_E("CpolarTunnel", "安装方式: curl -L https://www.cpolar.com/static/downloads/install-release-cpolar.sh | sudo bash");
    return false;
}

CpolarTunnel::CpolarTunnel(const CpolarConfig& cpolarConfig,
                            const EmailConfig& emailConfig,
                            int localPort)
    : cpolarConfig_(cpolarConfig), emailConfig_(emailConfig), localPort_(localPort) {
    CurlClient::Options opts;
    opts.connectTimeout = 3;
    opts.requestTimeout = 5;
    opts.verifySsl = false;
    apiClient_ = std::make_unique<CurlClient>(opts);
}

CpolarTunnel::~CpolarTunnel() {
    stop();
}

bool CpolarTunnel::checkBinary() {
    std::string cmd = resolvedBinPath_ + " version 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        LOG_E("CpolarTunnel", "cpolar 不可执行: " + resolvedBinPath_);
        return false;
    }
    char buf[256];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    int status = pclose(pipe);
    if (status != 0 || output.empty()) {
        LOG_E("CpolarTunnel", "cpolar 命令执行失败: " + resolvedBinPath_);
        return false;
    }
    LOG_I("CpolarTunnel", "cpolar 已就绪: " + output.substr(0, output.find_first_of("\r\n")));
    return true;
}

bool CpolarTunnel::configureAuthtoken() {
    if (cpolarConfig_.authtoken.empty()) {
        LOG_E("CpolarTunnel", "未配置 cpolar authtoken，请在 config.json 中设置 cpolar.authtoken");
        return false;
    }

    std::string cmd = resolvedBinPath_ + " authtoken " + cpolarConfig_.authtoken + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        LOG_E("CpolarTunnel", "执行 authtoken 命令失败");
        return false;
    }
    char buf[512];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    int status = pclose(pipe);

    if (status != 0 || output.find("error") != std::string::npos ||
        output.find("Error") != std::string::npos) {
        LOG_E("CpolarTunnel", "authtoken 配置失败: " + output);
        return false;
    }

    LOG_I("CpolarTunnel", "authtoken 配置成功");
    return true;
}

std::string CpolarTunnel::fetchUrlFromApi() {
    std::string baseUrl = "http://127.0.0.1:" + std::to_string(CPOLAR_API_PORT);
    auto resp = apiClient_->get(baseUrl + "/api/tunnels");
    if (!resp.success || resp.body.empty()) return {};

    try {
        auto j = nlohmann::json::parse(resp.body);
        nlohmann::json tunnels;
        if (j.is_array()) {
            tunnels = j;
        } else if (j.is_object()) {
            if (j.contains("tunnels")) tunnels = j["tunnels"];
            else if (j.contains("data")) tunnels = j["data"];
        }

        if (!tunnels.is_array()) return {};

        std::string httpUrl;
        for (const auto& t : tunnels) {
            std::string url = t.value("public_url", "");
            if (url.empty()) url = t.value("url", "");
            url = cleanUrl(url);
            if (url.find("https://") == 0) return url;
            if (url.find("http://") == 0 && httpUrl.empty()) httpUrl = url;
        }
        return httpUrl;
    } catch (...) {}

    return {};
}

std::string CpolarTunnel::fetchUrlFromDashboard() {
    std::string baseUrl = "http://127.0.0.1:" + std::to_string(CPOLAR_API_PORT);
    auto resp = apiClient_->get(baseUrl + "/");
    if (!resp.success || resp.body.empty()) return {};

    auto pos = resp.body.find("cpolar.cn");
    if (pos == std::string::npos) return {};

    auto start = resp.body.rfind("http", pos);
    if (start == std::string::npos) return {};

    auto end = pos + 10;
    while (end < resp.body.size() &&
           resp.body[end] != '"' && resp.body[end] != '\'' &&
           resp.body[end] != '<' && resp.body[end] != ' ' &&
           resp.body[end] != '`' && resp.body[end] != '\\' &&
           resp.body[end] != '\n')
        ++end;

    std::string url = resp.body.substr(start, end - start);
    url = cleanUrl(url);
    if (!url.empty() && url.find("cpolar.") != std::string::npos)
        return url;

    return {};
}

void CpolarTunnel::notifyUrl(const std::string& url, bool changed) {
    if (!emailConfig_.enabled || emailConfig_.accounts.empty()) return;

    std::string subject = changed ? "NarMusic 公网地址已变更" : "NarMusic 内网穿透已启动";
    std::string body = changed
        ? "NarMusic 的 cpolar 公网地址已变更，新地址如下：\n\n" + url + "\n\n本机地址：http://localhost:" + std::to_string(localPort_)
        : "NarMusic 的 cpolar 内网穿透已启动，公网地址如下：\n\n" + url + "\n\n本机地址：http://localhost:" + std::to_string(localPort_);

    EmailSender::sendAll(emailConfig_, subject, body);
}

bool CpolarTunnel::launchAndCapture() {
    std::string cmd = resolvedBinPath_ + " http";
    cmd += " -daemon=on";
    cmd += " -dashboard=on";
    cmd += " -inspect-addr=127.0.0.1:" + std::to_string(CPOLAR_API_PORT);
    cmd += " -region=" + cpolarConfig_.region;
    cmd += " -log=none";

    if (!cpolarConfig_.subdomain.empty()) {
        cmd += " -subdomain=" + cpolarConfig_.subdomain;
    }

    cmd += " " + std::to_string(localPort_);
    cmd += " >/dev/null 2>&1 &";

    LOG_I("CpolarTunnel", "启动 cpolar: " + cmd);

    int ret = system(cmd.c_str());
    if (ret != 0) {
        LOG_E("CpolarTunnel", "cpolar 启动失败，退出码: " + std::to_string(ret));
        return false;
    }

    LOG_I("CpolarTunnel", "cpolar 已后台启动，等待隧道建立...");

    for (int i = 0; i < 40; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::string url = fetchUrlFromApi();
        if (url.empty() && i >= 5)
            url = fetchUrlFromDashboard();

        if (!url.empty()) {
            publicUrl_ = url;
            running_ = true;
            LOG_I("CpolarTunnel", "公网访问地址: " + publicUrl_);
            printBanner();
            notifyUrl(publicUrl_, false);

            monitorThread_ = std::thread(&CpolarTunnel::monitorLoop, this);
            monitorThread_.detach();

            return true;
        }

        if (i % 10 == 9) {
            LOG_I("CpolarTunnel", "仍在等待隧道建立... (" + std::to_string(i + 1) + "秒)");
        }
    }

    LOG_W("CpolarTunnel", "等待隧道建立超时，cpolar 可能仍在后台创建中");
    return false;
}

void CpolarTunnel::monitorLoop() {
    int interval = cpolarConfig_.monitor_interval;
    if (interval < 10) interval = 10;
    LOG_I("CpolarTunnel", "URL 监控已启动 (每 " + std::to_string(interval) + " 秒检测)");

    while (running_) {
        for (int i = 0; i < interval && running_; ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));

        if (!running_) break;

        std::string url = fetchUrlFromApi();
        if (url.empty())
            url = fetchUrlFromDashboard();

        if (url.empty()) {
            LOG_W("CpolarTunnel", "未能获取隧道URL，cpolar 可能已断开");
            continue;
        }

        if (url != publicUrl_) {
            std::string oldUrl = publicUrl_;
            publicUrl_ = url;
            LOG_W("CpolarTunnel", "公网地址已变更!");
            LOG_I("CpolarTunnel", "旧地址: " + oldUrl);
            LOG_I("CpolarTunnel", "新地址: " + publicUrl_);

            printBanner();
            notifyUrl(publicUrl_, true);
        }
    }

    LOG_I("CpolarTunnel", "URL 监控已停止");
}

void CpolarTunnel::printBanner() {
    if (publicUrl_.empty()) return;

    auto displayWidth = [](const std::string& s) -> size_t {
        size_t w = 0;
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x80) { w += 1; i += 1; }
            else if ((c & 0xE0) == 0xC0) { w += 2; i += 2; }
            else if ((c & 0xF0) == 0xE0) { w += 2; i += 3; }
            else if ((c & 0xF8) == 0xF0) { w += 2; i += 4; }
            else { w += 1; i += 1; }
        }
        return w;
    };

    std::string line1 = "  cpolar 内网穿透已启动";
    std::string line2 = "  公网地址: " + publicUrl_;
    std::string line3 = "  本机仍可访问: http://localhost:" + std::to_string(localPort_);

    size_t contentW = std::max({displayWidth(line1), displayWidth(line2), displayWidth(line3)}) + 2;

    auto padRight = [&](const std::string& s) {
        std::cout << s;
        for (size_t i = displayWidth(s); i < contentW; ++i) std::cout << ' ';
    };

    std::string topBot(contentW, '=');

    std::cout << std::endl;
    std::cout << "+" << topBot << "+" << std::endl;
    std::cout << "|"; padRight(line1); std::cout << "|" << std::endl;
    std::cout << "|" << std::string(contentW, ' ') << "|" << std::endl;
    std::cout << "|"; padRight(line2); std::cout << "|" << std::endl;
    std::cout << "|" << std::string(contentW, ' ') << "|" << std::endl;
    std::cout << "|"; padRight(line3); std::cout << "|" << std::endl;
    std::cout << "+" << topBot << "+" << std::endl;
    std::cout << std::endl;
}

bool CpolarTunnel::start() {
    if (running_) return true;

    LOG_I("CpolarTunnel", "正在启动 cpolar 内网穿透...");

    if (!resolveBinary()) return false;
    if (!checkBinary()) return false;
    if (!configureAuthtoken()) return false;
    if (!launchAndCapture()) return false;

    return true;
}

void CpolarTunnel::stop() {
    if (!running_) return;

    LOG_I("CpolarTunnel", "正在停止 cpolar...");

    running_ = false;
    int ret = system("pkill -f 'cpolar http' 2>/dev/null");
    (void)ret;

    publicUrl_.clear();
    LOG_I("CpolarTunnel", "cpolar 已停止");
}

} // namespace narnat
