#ifndef NARNAT_TASK_H
#define NARNAT_TASK_H

#include <string>
#include <chrono>
#include <atomic>
#include "nlohmann/json.hpp"

namespace narnat {

enum class TaskStatus {
    Pending,
    Downloading,
    Finished,
    Failed
};

class Task {
public:
    Task() = default;
    Task(const std::string& id, const std::string& url, const std::string& filePath,
         const std::string& displayName, int delayMs = 0);

    // 身份
    const std::string& id() const { return id_; }
    const std::string& url() const { return url_; }

    // 状态
    TaskStatus status() const { return status_; }
    void setStatus(TaskStatus s) { status_ = s; }
    bool isFinished() const { return status_ == TaskStatus::Finished; }
    bool isFailed() const { return status_ == TaskStatus::Failed; }
    bool isDownloading() const { return status_ == TaskStatus::Downloading; }

    // 进度
    long long downloadedBytes() const { return downloadedBytes_; }
    void setDownloadedBytes(long long bytes) { downloadedBytes_ = bytes; }

    // 文件
    const std::string& filePath() const { return filePath_; }
    const std::string& displayName() const { return displayName_; }
    int delayMs() const { return delayMs_; }

    // 时间
    std::chrono::system_clock::time_point createdAt() const { return createdAt_; }

    // 使用标记（防止正在播放时被清理）
    bool inUse() const { return inUse_; }
    void setInUse(bool v) { inUse_ = v; }

    // 序列化
    nlohmann::json toJson() const;

private:
    std::string id_;
    std::string url_;
    TaskStatus status_ = TaskStatus::Pending;
    long long downloadedBytes_ = 0;
    std::string filePath_;
    std::string displayName_;
    int delayMs_ = 0;
    std::chrono::system_clock::time_point createdAt_;
    bool inUse_ = false;
};

} // namespace narnat

#endif
