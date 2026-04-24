#ifndef NARNAT_LOGGER_H
#define NARNAT_LOGGER_H

#include <string>
#include <mutex>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace narnat {

class Logger {
public:
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };

    struct Config {
        Level level = Level::INFO;
        std::string file_path;
        size_t max_size_mb = 10;
        bool console_output = true;
    };

    static Logger& instance();

    void init(const Config& cfg);
    void log(Level level, const std::string& tag, const std::string& msg);

    // 便捷方法
    void debug(const std::string& tag, const std::string& msg);
    void info(const std::string& tag, const std::string& msg);
    void warn(const std::string& tag, const std::string& msg);
    void error(const std::string& tag, const std::string& msg);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;

    std::string levelToString(Level level);
    std::string formatMessage(Level level, const std::string& tag, const std::string& msg);
    void writeToFile(const std::string& formatted);
    void rotateIfNeeded();

    Config config_;
    std::mutex mutex_;
    std::ofstream file_;
    bool initialized_ = false;
};

} // namespace narnat

// 便捷宏
#define LOG_D(tag, msg) narnat::Logger::instance().debug(tag, msg)
#define LOG_I(tag, msg) narnat::Logger::instance().info(tag, msg)
#define LOG_W(tag, msg) narnat::Logger::instance().warn(tag, msg)
#define LOG_E(tag, msg) narnat::Logger::instance().error(tag, msg)

#endif
