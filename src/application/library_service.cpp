#include "library_service.h"
#include "core/logger.h"
#include <fstream>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <taglib/mp4/mp4file.h>
#include <taglib/mp4/mp4tag.h>
#pragma GCC diagnostic pop
#include <sstream>

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

bool LibraryService::deleteFiles(const std::vector<int>& ids) {
    for (int id : ids) {
        libraryRepo_->remove(id);
        LOG_I("LibrarySvc", "已删除音乐文件: id=" + std::to_string(id));
    }
    return true;
}

std::string LibraryService::getLyrics(const std::string& filename) {
    auto entry = libraryRepo_->findBySystemFilename(filename);
    if (entry) return entry->lyrics;
    return "";
}

std::vector<std::pair<std::string, std::vector<char>>> LibraryService::getFilesData(const std::vector<int>& ids) {
    std::vector<std::pair<std::string, std::vector<char>>> result;
    for (int id : ids) {
        auto entry = libraryRepo_->findById(id);
        if (!entry) continue;
        if (entry->filePath.empty()) continue;

        std::ifstream file(entry->filePath, std::ios::binary | std::ios::ate);
        if (!file) continue;

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> data(static_cast<size_t>(size));
        file.read(data.data(), size);

        if (!data.empty()) {
            std::string displayName;
            if (!entry->originalFilename.empty()) {
                displayName = entry->originalFilename;
            } else if (!entry->songName.empty()) {
                displayName = entry->songName;
            } else {
                displayName = entry->systemFilename;
            }
            if (displayName.find('.') == std::string::npos) {
                displayName += ".m4a";
            }
            libraryRepo_->markUsed(id);
            result.emplace_back(std::move(displayName), std::move(data));
        }
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> LibraryService::getFilesPaths(const std::vector<int>& ids) {
    std::vector<std::pair<std::string, std::string>> result;
    for (int id : ids) {
        auto entry = libraryRepo_->findById(id);
        if (!entry) continue;
        if (entry->filePath.empty()) continue;

        std::string displayName;
        if (!entry->originalFilename.empty()) {
            displayName = entry->originalFilename;
        } else if (!entry->songName.empty()) {
            displayName = entry->songName;
        } else {
            displayName = entry->systemFilename;
        }
        if (displayName.find('.') == std::string::npos) {
            displayName += ".m4a";
        }
        libraryRepo_->markUsed(id);
        result.emplace_back(std::move(displayName), entry->filePath);
    }
    return result;
}

std::string LibraryService::generatePlaylist(const std::vector<int>& ids) {
    std::ostringstream playlist;
    int count = 0;
    for (int id : ids) {
        auto entry = libraryRepo_->findById(id);
        if (!entry) continue;
        if (entry->filePath.empty()) continue;

        TagLib::MP4::File file(entry->filePath.c_str());
        if (!file.isValid()) continue;
        auto* tag = file.tag();
        if (!tag) continue;

        if (tag->contains("----:com.narnat:narmeta")) {
            auto item = tag->item("----:com.narnat:narmeta");
            std::string val = item.toStringList().toString().to8Bit(true);
            if (!val.empty()) {
                playlist << val << "\n";
                count++;
            }
        }
    }

    LOG_I("LibrarySvc", "歌单生成: " + std::to_string(count) + " 条narmeta记录");
    return playlist.str();
}

} // namespace narnat
