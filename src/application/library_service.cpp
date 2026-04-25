#include "library_service.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"

namespace narnat {

LibraryService::LibraryService(std::shared_ptr<IMusicFileRepository> fileRepo,
                                const DownloadConfig& config)
    : fileRepo_(std::move(fileRepo)), config_(config) {}

nlohmann::json LibraryService::listFiles() {
    auto files = fileRepo_->scanLibrary(config_.path);

    nlohmann::json result = nlohmann::json::array();
    for (const auto& f : files) {
        nlohmann::json item;
        item["custom_filename"] = f.customFilename;
        item["system_filename"] = f.systemFilename;
        item["download_time"] = f.downloadTime;
        item["delay_ms"] = f.delayMs;
        item["file_size"] = f.fileSize;
        result.push_back(item);
    }

    LOG_D("LibrarySvc", "音乐库文件数: " + std::to_string(files.size()));
    return result;
}

} // namespace narnat
