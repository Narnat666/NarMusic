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
    j["is_finished"] = isFinished();
    j["is_success"] = (status_ == TaskStatus::Finished);
    j["downloaded_bytes"] = downloadedBytes_;
    return j;
}

} // namespace narnat
