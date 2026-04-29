#include "request.h"
#include <sstream>
#include <algorithm>

namespace narnat {

bool Request::isCompleteRequest(const std::string& data) {
    size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;

    size_t bodyStart = headerEnd + 4;
    size_t headerLineEnd = data.find("\r\n");
    if (headerLineEnd == std::string::npos) return false;

    size_t headerSectionStart = headerLineEnd + 2;
    std::string headersStr = data.substr(headerSectionStart, headerEnd - headerSectionStart);

    size_t contentLength = 0;
    std::istringstream stream(headersStr);
    std::string line;
    while (std::getline(stream, line, '\n')) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            if (!key.empty() && key.back() == '\r') key.pop_back();
            if (key == "Content-Length") {
                std::string val = line.substr(colonPos + 2);
                if (!val.empty() && val.back() == '\r') val.pop_back();
                try { contentLength = std::stoul(val); } catch (...) {}
            }
        }
    }

    size_t receivedBody = data.size() - bodyStart;
    return receivedBody >= contentLength;
}

bool Request::parse(const std::string& raw) {
    if (raw.empty()) return false;

    // 解析请求行
    size_t endLine = raw.find("\r\n");
    if (endLine == std::string::npos) return false;

    std::string requestLine = raw.substr(0, endLine);
    size_t firstSpace = requestLine.find(' ');
    if (firstSpace == std::string::npos) return false;

    std::string methodStr = requestLine.substr(0, firstSpace);
    if (methodStr == "GET") method_ = Method::GET;
    else if (methodStr == "POST") method_ = Method::POST;
    else if (methodStr == "PUT") method_ = Method::PUT;
    else if (methodStr == "DELETE") method_ = Method::DELETE;
    else method_ = Method::UNKNOWN;

    size_t secondSpace = requestLine.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos) return false;

    std::string fullPath = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);

    // 分离路径和查询字符串
    size_t queryPos = fullPath.find('?');
    if (queryPos != std::string::npos) {
        path_ = fullPath.substr(0, queryPos);
        queryString_ = fullPath.substr(queryPos + 1);
    } else {
        path_ = fullPath;
    }

    // 解析头部
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;

    std::string headersStr = raw.substr(endLine + 2, headerEnd - endLine - 2);
    std::istringstream headerStream(headersStr);
    std::string line;

    while (std::getline(headerStream, line, '\n')) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 2);
            // 去除\r
            if (!value.empty() && value.back() == '\r') value.pop_back();
            headers_[key] = value;
        }
    }

    // Content-Length
    bodyLength_ = 0;
    auto it = headers_.find("Content-Length");
    if (it != headers_.end()) {
        try { bodyLength_ = std::stoul(it->second); } catch (...) { bodyLength_ = 0; }
    }

    // Body
    if (bodyLength_ > 0) {
        body_ = raw.substr(headerEnd + 4, bodyLength_);
    }

    return true;
}

std::string Request::methodString() const {
    switch (method_) {
        case Method::GET: return "GET";
        case Method::POST: return "POST";
        case Method::PUT: return "PUT";
        case Method::DELETE: return "DELETE";
        default: return "UNKNOWN";
    }
}

std::string Request::header(const std::string& key) const {
    auto it = headers_.find(key);
    return (it != headers_.end()) ? it->second : "";
}

std::string Request::queryParam(const std::string& key) const {
    if (queryString_.empty()) return "";

    std::string search = key + "=";
    size_t pos = queryString_.find(search);
    if (pos == std::string::npos) return "";

    std::string value = queryString_.substr(pos + search.length());
    size_t end = value.find('&');
    if (end != std::string::npos) value = value.substr(0, end);

    return urlDecode(value);
}

std::string Request::pathParam(const std::string& key) const {
    auto it = pathParams_.find(key);
    return (it != pathParams_.end()) ? it->second : "";
}

std::string Request::rangeString() const {
    return header("Range");
}

std::string Request::urlDecode(const std::string& str) {
    std::string decoded;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int hex;
            std::stringstream ss;
            ss << std::hex << str.substr(i + 1, 2);
            ss >> hex;
            decoded += static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

} // namespace narnat
