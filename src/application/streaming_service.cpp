#include "streaming_service.h"
#include "core/logger.h"
#include <fstream>

namespace narnat {

StreamingService::StreamingService(std::shared_ptr<ITaskRepository> taskRepo,
                                    std::shared_ptr<IMusicLibraryRepository> libraryRepo,
                                    const DownloadConfig& config)
    : taskRepo_(std::move(taskRepo))
    , libraryRepo_(std::move(libraryRepo))
    , config_(config) {}

std::string StreamingService::getFilePath(const std::string& taskIdOrFilename) {
    auto task = taskRepo_->findById(taskIdOrFilename);
    if (task && task->isFinished()) return task->filePath();

    auto libEntry = libraryRepo_->findBySystemFilename(taskIdOrFilename);
    if (libEntry) {
        libraryRepo_->markUsed(libEntry->id);
        return libEntry->filePath;
    }

    LOG_W("StreamSvc", "文件未在数据库中找到: " + taskIdOrFilename);
    return "";
}

StreamData StreamingService::stream(const std::string& taskIdOrFilename,
                                     const std::string& rangeHeader) {
    std::string filePath = getFilePath(taskIdOrFilename);
    if (filePath.empty()) return {};

    return streamSender_.read(filePath, rangeHeader);
}

std::vector<char> StreamingService::getFileData(const std::string& taskIdOrFilename) {
    std::string filePath = getFilePath(taskIdOrFilename);
    if (filePath.empty()) return {};

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) return {};

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> data(static_cast<size_t>(size));
    file.read(data.data(), size);
    return data;
}

std::string StreamingService::getDisplayName(const std::string& taskIdOrFilename) {
    auto task = taskRepo_->findById(taskIdOrFilename);
    if (task) return task->displayName();

    auto libEntry = libraryRepo_->findBySystemFilename(taskIdOrFilename);
    if (libEntry) {
        if (!libEntry->originalFilename.empty()) return libEntry->originalFilename;
        if (!libEntry->songName.empty()) return libEntry->songName;
    }

    return "";
}

} // namespace narnat
