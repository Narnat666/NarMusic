#include "static_file_handler.h"
#include "core/logger.h"
#include <fstream>
#include <algorithm>

namespace narnat {

StaticFileHandler::StaticFileHandler(const std::string& webDir) : webDir_(webDir) {}

bool StaticFileHandler::isStaticFileRequest(const std::string& path) const {
    if (path == "/" || path == "/index.html") return true;

    static const std::vector<std::string> extensions = {
        ".html", ".css", ".js", ".json", ".png", ".jpg", ".jpeg", ".ico", ".svg", ".woff", ".woff2"
    };

    for (const auto& ext : extensions) {
        if (path.size() >= ext.size() &&
            path.substr(path.size() - ext.size()) == ext) {
            return true;
        }
    }
    return false;
}

Response StaticFileHandler::handle(const Request& req) {
    std::string filePath = webDir_;
    if (req.path() == "/") {
        filePath += "/index.html";
    } else {
        filePath += req.path();
    }

    LOG_I("StaticFile", req.path() + " -> " + filePath);

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        LOG_W("StaticFile", "文件不存在: " + filePath);
        return Response::error(404, "Not Found", "file_not_found", "静态文件不存在: " + filePath);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    return Response::file(content, contentTypeForPath(filePath));
}

std::string StaticFileHandler::contentTypeForPath(const std::string& path) const {
    if (path.find(".html") != std::string::npos) return "text/html; charset=utf-8";
    if (path.find(".css") != std::string::npos)  return "text/css; charset=utf-8";
    if (path.find(".js") != std::string::npos)   return "application/javascript; charset=utf-8";
    if (path.find(".json") != std::string::npos)  return "application/json; charset=utf-8";
    if (path.find(".png") != std::string::npos)   return "image/png";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos)
        return "image/jpeg";
    if (path.find(".svg") != std::string::npos)   return "image/svg+xml";
    if (path.find(".ico") != std::string::npos)   return "image/x-icon";
    if (path.find(".woff2") != std::string::npos) return "font/woff2";
    if (path.find(".woff") != std::string::npos)  return "font/woff";
    return "application/octet-stream";
}

} // namespace narnat
