#include "task.h"
#include "nlohmann/json.hpp"

namespace narnat {

Task::Task(const std::string& id, const std::string& url, const std::string& filePath,
           const std::string& displayName, int delayMs)
    : id_(id), url_(url), filePath_(filePath), displayName_(displayName),
      delayMs_(delayMs), createdAt_(std::chrono::system_clock::now()) {}

nlohmann::json Task::toJson() const {
    nlohmann::json j;
    j["task_id"] = id_;
    j["url"] = url_;
    j["is_downloading"] = isDownloading();
    j["is_finished"] = (status_ == TaskStatus::Finished || status_ == TaskStatus::Failed);
    j["is_success"] = (status_ == TaskStatus::Finished);
    j["downloaded_bytes"] = downloadedBytes_;

    if (status_ == TaskStatus::Failed) {
        j["error"] = "download_failed";
    }

    if (status_ == TaskStatus::Finished) {
        nlohmann::json info;
        info["filename"] = displayName_;
        if (!displayName_.empty() && displayName_.find('.') == std::string::npos) {
            info["filename"] = displayName_ + ".m4a";
        }
        info["filesize"] = downloadedBytes_;
        j["file_info"] = info;
    }

    return j;
}

} // namespace narnat
