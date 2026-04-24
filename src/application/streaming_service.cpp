#include "streaming_service.h"
#include "core/logger.h"
#include <fstream>

namespace narnat {

StreamingService::StreamingService(std::shared_ptr<ITaskRepository> taskRepo,
                                    const DownloadConfig& config)
    : taskRepo_(std::move(taskRepo)), config_(config) {}

std::string StreamingService::getFilePath(const std::string& taskIdOrFilename) {
    // 先尝试按task_id查找
    auto task = taskRepo_->findById(taskIdOrFilename);
    if (task && task->isFinished()) return task->filePath();

    // 按文件名查找
    auto tasks = taskRepo_->findAll();
    for (const auto& t : tasks) {
        if (t.displayName() == taskIdOrFilename || t.filePath().find(taskIdOrFilename) != std::string::npos) {
            return t.filePath();
        }
    }

    // 直接作为文件路径
    return config_.path + taskIdOrFilename;
}

StreamData StreamingService::stream(const std::string& taskIdOrFilename,
                                     const std::string& rangeHeader) {
    std::string filePath = getFilePath(taskIdOrFilename);
    if (filePath.empty()) {
        LOG_W("StreamSvc", "文件未找到: " + taskIdOrFilename);
        return {};
    }

    return streamSender_.read(filePath, rangeHeader);
}

std::vector<char> StreamingService::getFileData(const std::string& taskIdOrFilename) {
    std::string filePath = getFilePath(taskIdOrFilename);

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) return {};

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> data(size);
    file.read(data.data(), size);
    return data;
}

} // namespace narnat
