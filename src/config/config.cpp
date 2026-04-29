#include "config.h"
#include "core/logger.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace narnat {

void Config::fromJson(const nlohmann::json& j) {
    if (j.contains("server")) {
        auto& s = j["server"];
        if (s.contains("port")) server.port = s["port"].get<int>();
        if (s.contains("backlog")) server.backlog = s["backlog"].get<int>();
        if (s.contains("connection_timeout")) server.connection_timeout = s["connection_timeout"].get<int>();
        if (s.contains("thread_pool_size")) server.thread_pool_size = s["thread_pool_size"].get<int>();
    }

    if (j.contains("download")) {
        auto& d = j["download"];
        if (d.contains("path")) download.path = d["path"].get<std::string>();
        if (d.contains("extension")) download.extension = d["extension"].get<std::string>();
        if (d.contains("cleanup_interval")) download.cleanup_interval = d["cleanup_interval"].get<int>();
        if (d.contains("max_age")) download.max_age = d["max_age"].get<int>();
    }

    if (j.contains("bilibili")) {
        auto& b = j["bilibili"];
        if (b.contains("user_agent")) bilibili.user_agent = b["user_agent"].get<std::string>();
        if (b.contains("connect_timeout")) bilibili.connect_timeout = b["connect_timeout"].get<int>();
        if (b.contains("request_timeout")) bilibili.request_timeout = b["request_timeout"].get<int>();
    }

    if (j.contains("lyrics")) {
        auto& l = j["lyrics"];
        if (l.contains("default_platform")) lyrics.default_platform = l["default_platform"].get<std::string>();
        if (l.contains("platforms")) {
            lyrics.platforms.clear();
            for (const auto& p : l["platforms"]) {
                lyrics.platforms.push_back(p.get<std::string>());
            }
        }
    }

    if (j.contains("logging")) {
        auto& lg = j["logging"];
        if (lg.contains("level")) logging.level = lg["level"].get<std::string>();
        if (lg.contains("file")) logging.file = lg["file"].get<std::string>();
        if (lg.contains("max_size_mb")) logging.max_size_mb = lg["max_size_mb"].get<size_t>();
    }

    if (j.contains("database")) {
        auto& db = j["database"];
        if (db.contains("path")) database.path = db["path"].get<std::string>();
    }

    if (j.contains("cpolar")) {
        auto& cp = j["cpolar"];
        if (cp.contains("enabled")) cpolar.enabled = cp["enabled"].get<bool>();
        if (cp.contains("authtoken")) cpolar.authtoken = cp["authtoken"].get<std::string>();
        if (cp.contains("subdomain")) cpolar.subdomain = cp["subdomain"].get<std::string>();
        if (cp.contains("region")) cpolar.region = cp["region"].get<std::string>();
        if (cp.contains("bin_path")) cpolar.bin_path = cp["bin_path"].get<std::string>();
        if (cp.contains("monitor_interval")) cpolar.monitor_interval = cp["monitor_interval"].get<int>();
    }

    if (j.contains("email")) {
        auto& em = j["email"];
        if (em.contains("enabled")) email.enabled = em["enabled"].get<bool>();
        if (em.contains("smtp_host")) email.smtp_host = em["smtp_host"].get<std::string>();
        if (em.contains("smtp_port")) email.smtp_port = em["smtp_port"].get<int>();
        if (em.contains("accounts") && em["accounts"].is_array()) {
            for (const auto& acc : em["accounts"]) {
                EmailAccount ea;
                if (acc.contains("sender")) ea.sender = acc["sender"].get<std::string>();
                if (acc.contains("password")) ea.password = acc["password"].get<std::string>();
                if (acc.contains("receiver")) ea.receiver = acc["receiver"].get<std::string>();
                if (!ea.sender.empty() && !ea.password.empty() && !ea.receiver.empty())
                    email.accounts.push_back(ea);
            }
        }
    }
}

Config Config::load(const std::string& path) {
    Config cfg;
    try {
        std::ifstream file(path);
        if (file.is_open()) {
            nlohmann::json j;
            file >> j;
            cfg.fromJson(j);
        }
    } catch (const std::exception& e) {
        std::cerr << "配置文件加载失败: " << e.what() << "，使用默认配置" << std::endl;
    }
    return cfg;
}

Config Config::loadDefault() {
    return Config();
}

void Config::applyOverrides(int port, const std::string& downloadPath,
                            const std::string& extension, bool debug,
                            const std::string& cpolarToken,
                            const std::string& emailKey) {
    if (port > 0) server.port = port;
    if (!downloadPath.empty()) download.path = downloadPath;
    if (!extension.empty()) download.extension = extension;
    debug_mode = debug;
    if (!cpolarToken.empty()) {
        cpolar.authtoken = cpolarToken;
        cpolar.enabled = true;
    }
    if (!emailKey.empty()) {
        auto sep = emailKey.find(':');
        if (sep != std::string::npos) {
            EmailAccount ea;
            ea.sender = emailKey.substr(0, sep);
            ea.receiver = ea.sender;
            ea.password = emailKey.substr(sep + 1);
            email.accounts.insert(email.accounts.begin(), ea);
        } else {
            if (email.accounts.empty()) {
                EmailAccount ea;
                ea.password = emailKey;
                email.accounts.push_back(ea);
            } else {
                email.accounts[0].password = emailKey;
            }
        }
        email.enabled = true;
    }
}

} // namespace narnat
