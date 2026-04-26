#include "library_service.h"
#include "core/logger.h"

namespace narnat {

LibraryService::LibraryService(std::shared_ptr<IMusicLibraryRepository> libraryRepo)
    : libraryRepo_(std::move(libraryRepo)) {}

nlohmann::json LibraryService::listFiles() {
    auto entries = libraryRepo_->findAll();

    nlohmann::json result = nlohmann::json::array();
    for (const auto& e : entries) {
        result.push_back(e.toJson());
    }

    LOG_D("LibrarySvc", "音乐库文件数: " + std::to_string(entries.size()));
    return result;
}

bool LibraryService::deleteFile(int id) {
    libraryRepo_->remove(id);
    LOG_I("LibrarySvc", "已删除音乐文件: id=" + std::to_string(id));
    return true;
}

} // namespace narnat
