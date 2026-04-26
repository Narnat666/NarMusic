#include "library_controller.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"

namespace narnat {

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

} // namespace narnat
