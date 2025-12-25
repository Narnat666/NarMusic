#include "http_request.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <unistd.h>

HttpRequest::HttpRequest(int socket) : socket_(socket), body_length_(0) {}

bool HttpRequest::parse() {
    char buffer[4096];
    ssize_t bytes_read = read(socket_, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return false;
    
    buffer[bytes_read] = '\0';
    std::string request(buffer);
    
    // 解析请求行
    size_t end_line = request.find("\r\n");
    if (end_line == std::string::npos) return false;
    
    std::string request_line = request.substr(0, end_line);
    size_t space_pos = request_line.find(' ');
    if (space_pos == std::string::npos) return false;
    
    method_ = request_line.substr(0, space_pos);
    path_ = request_line.substr(space_pos + 1, request_line.find(' ', space_pos + 1) - space_pos - 1);
    
    // 解析头部
    size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;
    
    std::string headers_str = request.substr(end_line + 2, header_end - end_line - 2);
    std::istringstream header_stream(headers_str);
    std::string header;
    
    while (std::getline(header_stream, header, '\n')) {
        size_t colon_pos = header.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = header.substr(0, colon_pos);
            std::string value = header.substr(colon_pos + 2);
            headers_[key] = value;
        }
    }
    
    // 修复：安全获取 Content-Length
    body_length_ = 0;
    auto content_len_it = headers_.find("Content-Length");
    if (content_len_it != headers_.end()) {
        try {
            body_length_ = std::stoi(content_len_it->second);
        } catch (...) {
            body_length_ = 0;
        }
    }
    
    // 获取请求体
    if (body_length_ > 0) {
        body_ = request.substr(header_end + 4, body_length_);
    }
    
    return true;
}

std::string HttpRequest::getMethod() const { return method_; }
std::string HttpRequest::getPath() const { return path_; }
std::string HttpRequest::getHeader(const std::string& key) const {
    auto it = headers_.find(key);
    return (it != headers_.end()) ? it->second : "";
}
std::string HttpRequest::getBody() const { return body_; }
int HttpRequest::getBodyLength() const { return body_length_; }