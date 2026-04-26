#include "library_controller.h"
#include "core/logger.h"
#include "infrastructure/filesystem/zip_writer.h"
#include "nlohmann/json.hpp"
#include <sstream>
#include <set>
#include <iomanip>

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

        if (ids.size() == 1) {
            auto filesData = libraryService_->getFilesData(ids);
            if (filesData.empty()) {
                return Response::error(404, "Not Found", "file_not_found", "文件不存在");
            }
            return Response::download(filesData[0].second, filesData[0].first);
        }

        auto filesData = libraryService_->getFilesData(ids);
        if (filesData.empty()) {
            return Response::error(404, "Not Found", "files_not_found", "文件均不存在");
        }

        std::vector<ZipEntry> entries;
        std::set<std::string> usedNames;
        for (auto& [name, data] : filesData) {
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
            entries.push_back({uniqueName, std::move(data)});
        }

        auto zipData = ZipWriter::create(entries);

        std::ostringstream dateStr;
        auto now = std::time(nullptr);
        auto* t = std::localtime(&now);
        dateStr << "NarMusic_"
                << (1900 + t->tm_year)
                << std::setfill('0') << std::setw(2) << (t->tm_mon + 1)
                << std::setw(2) << t->tm_mday << "_"
                << std::setw(2) << t->tm_hour
                << std::setw(2) << t->tm_min
                << std::setw(2) << t->tm_sec
                << ".zip";

        return Response::download(zipData, dateStr.str());

    } catch (const std::exception& e) {
        LOG_E("LibraryCtrl", std::string("批量下载失败: ") + e.what());
        return Response::error(400, "Bad Request", "parse_error", e.what());
    }
}

} // namespace narnat
