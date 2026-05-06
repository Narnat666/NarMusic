#include "static_file_handler.h"
#include "core/logger.h"
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <zlib.h>

namespace narnat {

static std::string gzipCompress(const std::string& data) {
    z_stream strm = {};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                 15 + 16, 8, Z_DEFAULT_STRATEGY);
#pragma GCC diagnostic pop

    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    strm.avail_in = static_cast<uInt>(data.size());

    size_t bound = deflateBound(&strm, data.size());
    std::string out(bound, '\0');

    strm.next_out = reinterpret_cast<Bytef*>(&out[0]);
    strm.avail_out = static_cast<uInt>(bound);

    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        return {};
    }

    out.resize(strm.total_out);
    return out;
}

StaticFileHandler::StaticFileHandler(const std::string& webDir) : webDir_(webDir) {
    namespace fs = std::filesystem;
    try {
        webDirCanonical_ = fs::canonical(webDir_).string();
    } catch (...) {
        webDirCanonical_ = fs::absolute(webDir_).string();
    }
}

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
    namespace fs = std::filesystem;

    fs::path requestedPath = (req.path() == "/")
        ? fs::path(webDir_) / "index.html"
        : fs::path(webDir_) / req.path().substr(1);

    fs::path resolvedPath;
    try {
        resolvedPath = fs::canonical(requestedPath);
    } catch (...) {
        LOG_W("StaticFile", "路径解析失败: " + requestedPath.string());
        return Response::error(404, "Not Found", "file_not_found", "文件不存在");
    }

    if (resolvedPath.string().find(webDirCanonical_) != 0) {
        LOG_W("StaticFile", "路径越界: " + resolvedPath.string());
        return Response::error(403, "Forbidden", "forbidden", "禁止访问");
    }

    LOG_I("StaticFile", req.path() + " -> " + resolvedPath.string());

    std::ifstream file(resolvedPath, std::ios::binary);
    if (!file) {
        return Response::error(404, "Not Found", "file_not_found", "文件不存在");
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    std::string contentType = contentTypeForPath(resolvedPath.string());
    auto resp = Response::file(content, contentType);

    bool acceptsGzip = req.header("Accept-Encoding").find("gzip") != std::string::npos;
    bool isCompressible = contentType.find("text/") == 0 ||
                          contentType.find("application/javascript") == 0 ||
                          contentType.find("application/json") == 0;
    if (acceptsGzip && isCompressible && content.size() > 256) {
        std::string compressed = gzipCompress(content);
        if (!compressed.empty()) {
            resp = Response::file(compressed, contentType);
            resp.setHeader("Content-Encoding", "gzip");
            resp.setHeader("Vary", "Accept-Encoding");
            LOG_I("StaticFile", req.path() + " gzip " + std::to_string(content.size())
                  + " -> " + std::to_string(compressed.size()) + " bytes ("
                  + std::to_string(compressed.size() * 100 / content.size()) + "%)");
        } else {
            LOG_W("StaticFile", req.path() + " gzip压缩失败，返回原始内容");
        }
    }

    return resp;
}

std::string StaticFileHandler::contentTypeForPath(const std::string& path) const {
    if (path.find(".html") != std::string::npos) return "text/html; charset=utf-8";
    if (path.find(".css") != std::string::npos)  return "text/css; charset=utf-8";
    if (path.find(".json") != std::string::npos)  return "application/json; charset=utf-8";
    if (path.find(".js") != std::string::npos)   return "application/javascript; charset=utf-8";
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
