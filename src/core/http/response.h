#ifndef NARNAT_RESPONSE_H
#define NARNAT_RESPONSE_H

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "nlohmann/json.hpp"

namespace narnat {

class Response {
public:
    Response() = default;

    // JSON响应
    static Response json(int status, const std::string& statusText,
                         const nlohmann::json& body);

    // 错误响应
    static Response error(int status, const std::string& statusText,
                          const std::string& code, const std::string& msg);

    // 静态文件响应
    static Response file(const std::string& content, const std::string& contentType);

    // 文件下载响应
    static Response download(const std::vector<char>& fileData,
                             const std::string& displayName);

    // 流式音频响应（Range）
    static Response stream(const std::vector<char>& data,
                           long long fileSize,
                           long long rangeStart,
                           long long rangeEnd,
                           bool isPartial);

    // 设置自定义头
    Response& setHeader(const std::string& key, const std::string& value);

    // 序列化为HTTP响应字符串
    std::string serialize() const;

    int status() const { return status_; }

private:
    int status_ = 200;
    std::string statusText_ = "OK";
    std::string body_;
    std::map<std::string, std::string> headers_;
    bool isBinary_ = false;

    static std::string contentTypeForFile(const std::string& path);
};

} // namespace narnat

#endif
