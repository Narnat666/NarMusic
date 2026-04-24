#include "logger.h"
#include <iostream>
#include <filesystem>

namespace narnat {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::init(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = cfg;

    if (!config_.file_path.empty()) {
        namespace fs = std::filesystem;
        fs::path p(config_.file_path);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
        file_.open(config_.file_path, std::ios::app);
    }
    initialized_ = true;
}

std::string Logger::levelToString(Level level) {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO";
        case Level::WARN:  return "WARN";
        case Level::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

std::string Logger::formatMessage(Level level, const std::string& tag, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);

    std::ostringstream oss;
    oss << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
        << "[" << levelToString(level) << "] "
        << "[" << tag << "] " << msg;
    return oss.str();
}

void Logger::rotateIfNeeded() {
    if (config_.file_path.empty() || !file_.is_open()) return;

    try {
        namespace fs = std::filesystem;
        auto size = fs::file_size(config_.file_path);
        if (size > config_.max_size_mb * 1024 * 1024) {
            file_.close();
            std::string backup = config_.file_path + ".1";
            fs::rename(config_.file_path, backup);
            file_.open(config_.file_path, std::ios::app);
        }
    } catch (...) {}
}

void Logger::writeToFile(const std::string& formatted) {
    if (file_.is_open()) {
        file_ << formatted << std::endl;
        file_.flush();
        rotateIfNeeded();
    }
}

void Logger::log(Level level, const std::string& tag, const std::string& msg) {
    if (!initialized_) {
        // 未初始化时直接输出到stderr
        std::cerr << "[" << levelToString(level) << "] [" << tag << "] " << msg << std::endl;
        return;
    }

    if (level < config_.level) return;

    std::string formatted = formatMessage(level, tag, msg);

    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.console_output) {
        if (level >= Level::WARN) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }
    writeToFile(formatted);
}

void Logger::debug(const std::string& tag, const std::string& msg) { log(Level::DEBUG, tag, msg); }
void Logger::info(const std::string& tag, const std::string& msg)  { log(Level::INFO, tag, msg); }
void Logger::warn(const std::string& tag, const std::string& msg)  { log(Level::WARN, tag, msg); }
void Logger::error(const std::string& tag, const std::string& msg) { log(Level::ERROR, tag, msg); }

} // namespace narnat
