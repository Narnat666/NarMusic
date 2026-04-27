#ifndef NARNAT_CONFIG_H
#define NARNAT_CONFIG_H

#include <string>
#include <vector>
#include "nlohmann/json.hpp"

namespace narnat {

struct ServerConfig {
    int port = 8080;
    int backlog = 512;
    int connection_timeout = 30;
    int thread_pool_size = 5;
};

struct DownloadConfig {
    std::string path = "./download/";
    std::string extension = ".m4a";
    int cleanup_interval = 600;
    int max_age = 600;
};

struct BilibiliConfig {
    std::string user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
    int connect_timeout = 10;
    int request_timeout = 15;
};

struct LyricsConfig {
    std::string default_platform = "酷狗音乐";
    std::vector<std::string> platforms = {"酷狗音乐", "网易云音乐", "QQ音乐", "汽水音乐"};
};

struct LogConfig {
    std::string level = "info";
    std::string file = "./logs/narnat.log";
    size_t max_size_mb = 10;
};

struct DatabaseConfig {
    std::string path = "./data/narnat.db";
};

class Config {
public:
    static Config load(const std::string& path);
    static Config loadDefault();

    // 命令行参数覆盖
    void applyOverrides(int port, const std::string& downloadPath,
                        const std::string& extension, bool debug);

    ServerConfig server;
    DownloadConfig download;
    BilibiliConfig bilibili;
    LyricsConfig lyrics;
    LogConfig logging;
    DatabaseConfig database;
    bool debug_mode = false;

private:
    void fromJson(const nlohmann::json& j);
};

} // namespace narnat

#endif
