#include "library_controller.h"
#include "core/logger.h"
#include "infrastructure/filesystem/zip_writer.h"
#include "nlohmann/json.hpp"
#include <sstream>
#include <set>
#include <iomanip>
#include <unistd.h>
#include <sys/stat.h>

namespace narnat {

using json = nlohmann::json;

LibraryController::LibraryController(std::shared_ptr<LibraryService> libraryService)
    : libraryService_(std::move(libraryService)) {}

Response LibraryController::list(const Request& /*req*/) {
    auto files = libraryService_->listFiles();
    return Response::json(200, "OK", files);
}

Response LibraryController::remove(const Request& req) {
    std::string idStr = req.queryParam("id");
    if (idStr.empty()) {
        return Response::error(400, "Bad Request", "missing_id", "id参数缺失");
    }

    int id = 0;
    try { id = std::stoi(idStr); } catch (...) {
        return Response::error(400, "Bad Request", "invalid_id", "id参数无效");
    }

    if (libraryService_->deleteFile(id)) {
        nlohmann::json result;
        result["message"] = "deleted";
        result["id"] = id;
        return Response::json(200, "OK", result);
    }

    return Response::error(404, "Not Found", "not_found", "文件不存在");
}

Response LibraryController::batchRemove(const Request& req) {
    try {
        json body = json::parse(req.body());

        if (!body.contains("ids") || !body["ids"].is_array()) {
            return Response::error(400, "Bad Request", "missing_ids", "ids参数缺失或格式错误");
        }

        std::vector<int> ids;
        for (const auto& id : body["ids"]) {
            ids.push_back(id.get<int>());
        }

        if (ids.empty()) {
            return Response::error(400, "Bad Request", "empty_ids", "ids不能为空");
        }

        libraryService_->deleteFiles(ids);

        json result;
        result["message"] = "deleted";
        result["count"] = ids.size();
        return Response::json(200, "OK", result);

    } catch (const std::exception& e) {
        LOG_E("LibraryCtrl", std::string("批量删除失败: ") + e.what());
        return Response::error(400, "Bad Request", "parse_error", e.what());
    }
}

Response LibraryController::batchDownload(const Request& req) {
    try {
        json body = json::parse(req.body());

        if (!body.contains("ids") || !body["ids"].is_array()) {
            return Response::error(400, "Bad Request", "missing_ids", "ids参数缺失或格式错误");
        }

        std::vector<int> ids;
        for (const auto& id : body["ids"]) {
            ids.push_back(id.get<int>());
        }

        if (ids.empty()) {
            return Response::error(400, "Bad Request", "empty_ids", "ids不能为空");
        }

        auto filesPaths = libraryService_->getFilesPaths(ids);
        if (filesPaths.empty()) {
            return Response::error(404, "Not Found", "files_not_found", "文件均不存在");
        }

        if (ids.size() == 1) {
            FileStreamInfo info;
            info.filePath = filesPaths[0].second;
            struct stat st;
            if (stat(info.filePath.c_str(), &st) != 0) {
                return Response::error(404, "Not Found", "file_not_found", "文件不存在");
            }
            info.fileSize = st.st_size;
            info.rangeStart = 0;
            info.rangeEnd = st.st_size - 1;
            return Response::downloadFile(info, filesPaths[0].first);
        }

        std::ostringstream dateStr;
        auto now = std::time(nullptr);
        auto* t = std::localtime(&now);
        dateStr << "NarMusic_"
                << (1900 + t->tm_year)
                << std::setfill('0') << std::setw(2) << (t->tm_mon + 1)
                << std::setw(2) << t->tm_mday << "_"
                << std::setw(2) << t->tm_hour
                << std::setw(2) << t->tm_min
                << std::setw(2) << t->tm_sec;
        std::string zipPath = "/tmp/narnat_" + dateStr.str() + ".zip";
        std::string zipName = dateStr.str() + ".zip";

        std::vector<ZipEntry> entries;
        std::set<std::string> usedNames;
        for (auto& [name, path] : filesPaths) {
            std::string uniqueName = name;
            int counter = 1;
            while (usedNames.count(uniqueName)) {
                auto dotPos = name.rfind('.');
                if (dotPos != std::string::npos) {
                    uniqueName = name.substr(0, dotPos) + " (" + std::to_string(counter++) + ")" + name.substr(dotPos);
                } else {
                    uniqueName = name + " (" + std::to_string(counter++) + ")";
                }
            }
            usedNames.insert(uniqueName);
            entries.push_back({uniqueName, path});
        }

        if (!ZipWriter::createToFile(entries, zipPath)) {
            unlink(zipPath.c_str());
            return Response::error(500, "Internal Error", "zip_failed", "ZIP打包失败");
        }

        FileStreamInfo info;
        info.filePath = zipPath;
        struct stat st;
        if (stat(zipPath.c_str(), &st) != 0) {
            unlink(zipPath.c_str());
            return Response::error(500, "Internal Error", "zip_stat_failed", "ZIP文件状态获取失败");
        }
        info.fileSize = st.st_size;
        info.rangeStart = 0;
        info.rangeEnd = st.st_size - 1;
        info.cleanupPath = zipPath;

        return Response::downloadFile(info, zipName);

    } catch (const std::exception& e) {
        LOG_E("LibraryCtrl", std::string("批量下载失败: ") + e.what());
        return Response::error(400, "Bad Request", "parse_error", e.what());
    }
}

Response LibraryController::lyrics(const Request& req) {
    std::string filename = req.queryParam("filename");
    if (filename.empty()) {
        return Response::error(400, "Bad Request", "missing_filename", "filename参数缺失");
    }

    std::string lyricsText = libraryService_->getLyrics(filename);
    if (lyricsText.empty()) {
        return Response::error(404, "Not Found", "no_lyrics", "歌词不存在");
    }

    nlohmann::json result;
    result["lyrics"] = lyricsText;
    return Response::json(200, "OK", result);
}

} // namespace narnat
