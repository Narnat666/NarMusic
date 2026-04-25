#include "response.h"
#include <iomanip>

namespace narnat {

// RFC 5987 percent-encoding for filename* parameter
static std::string urlEncodeUtf8(const std::string& s) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if (c < 128 && isalnum(c)) {
            escaped << c;
            continue;
        }
        if (c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }
        escaped << '%' << std::setw(2) << static_cast<int>(c);
    }
    return escaped.str();
}

Response Response::json(int status, const std::string& statusText,
                         const nlohmann::json& body) {
    Response r;
    r.status_ = status;
    r.statusText_ = statusText;
    r.body_ = body.dump();
    r.headers_["Content-Type"] = "application/json";
    r.headers_["Access-Control-Allow-Origin"] = "*";
    return r;
}

Response Response::error(int status, const std::string& statusText,
                          const std::string& code, const std::string& msg) {
    nlohmann::json body;
    body["error"] = code;
    body["message"] = msg;
    return json(status, statusText, body);
}

Response Response::file(const std::string& content, const std::string& contentType) {
    Response r;
    r.status_ = 200;
    r.statusText_ = "OK";
    r.body_ = content;
    r.headers_["Content-Type"] = contentType;
    r.headers_["Cache-Control"] = "max-age=3600";
    return r;
}

Response Response::download(const std::vector<char>& fileData,
                             const std::string& displayName) {
    Response r;
    r.status_ = 200;
    r.statusText_ = "OK";
    r.body_.assign(fileData.begin(), fileData.end());
    r.isBinary_ = true;
    r.headers_["Content-Type"] = "application/octet-stream";
    r.headers_["Content-Disposition"] = "attachment; filename=\"" + urlEncodeUtf8(displayName) + "\"; filename*=UTF-8''" + urlEncodeUtf8(displayName);
    return r;
}

Response Response::stream(const std::vector<char>& data,
                           long long fileSize,
                           long long rangeStart,
                           long long rangeEnd,
                           bool isPartial) {
    Response r;
    if (isPartial) {
        r.status_ = 206;
        r.statusText_ = "Partial Content";
        std::ostringstream rangeHeader;
        rangeHeader << "bytes " << rangeStart << "-" << rangeEnd << "/" << fileSize;
        r.headers_["Content-Range"] = rangeHeader.str();
    } else {
        r.status_ = 200;
        r.statusText_ = "OK";
    }
    r.body_.assign(data.begin(), data.end());
    r.isBinary_ = true;
    r.headers_["Content-Type"] = "audio/mp4";
    r.headers_["Accept-Ranges"] = "bytes";
    r.headers_["Connection"] = "close";
    r.headers_["Access-Control-Allow-Origin"] = "*";
    r.headers_["Cache-Control"] = "no-cache, no-store, must-revalidate";
    return r;
}

Response& Response::setHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

std::string Response::serialize() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_ << " " << statusText_ << "\r\n";

    for (const auto& [key, value] : headers_) {
        oss << key << ": " << value << "\r\n";
    }

    if (headers_.find("Content-Length") == headers_.end()) {
        oss << "Content-Length: " << body_.size() << "\r\n";
    }

    if (headers_.find("Connection") == headers_.end()) {
        oss << "Connection: close\r\n";
    }

    oss << "\r\n";
    oss << body_;
    return oss.str();
}

std::string Response::contentTypeForFile(const std::string& path) {
    if (path.find(".html") != std::string::npos) return "text/html; charset=utf-8";
    if (path.find(".css") != std::string::npos)  return "text/css; charset=utf-8";
    if (path.find(".js") != std::string::npos)   return "application/javascript; charset=utf-8";
    if (path.find(".json") != std::string::npos)  return "application/json";
    if (path.find(".png") != std::string::npos)   return "image/png";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos)
        return "image/jpeg";
    return "text/plain";
}

} // namespace narnat
