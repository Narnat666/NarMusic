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
        item["filename"] = f.systemFilename;
        item["size"] = f.fileSize;
        result.push_back(item);
    }

    LOG_D("LibrarySvc", "音乐库文件数: " + std::to_string(files.size()));
    return result;
}

} // namespace narnat
