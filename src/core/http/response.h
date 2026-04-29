#ifndef NARNAT_RESPONSE_H
#define NARNAT_RESPONSE_H

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "nlohmann/json.hpp"

namespace narnat {

struct FileStreamInfo {
    std::string filePath;
    long long fileSize = 0;
    long long rangeStart = 0;
    long long rangeEnd = 0;
    bool isPartial = false;
};

struct StreamData {
    std::vector<char> buffer;
    long long fileSize = 0;
    long long rangeStart = 0;
    long long rangeEnd = 0;
    long long requestSize = 0;
    bool isPartial = false;
};

class Response {
public:
    Response() = default;

    static Response json(int status, const std::string& statusText,
                         const nlohmann::json& body);

    static Response error(int status, const std::string& statusText,
                          const std::string& code, const std::string& msg);

    static Response file(const std::string& content, const std::string& contentType);

    static Response download(const std::vector<char>& fileData,
                             const std::string& displayName);

    static Response stream(const std::vector<char>& data,
                           long long fileSize,
                           long long rangeStart,
                           long long rangeEnd,
                           bool isPartial);

    static Response streamFile(const FileStreamInfo& info);

    Response& setHeader(const std::string& key, const std::string& value);

    std::string serialize() const;

    std::string serializeHeaders() const;

    bool isFileStream() const { return isFileStream_; }
    const FileStreamInfo& fileStreamInfo() const { return fileStreamInfo_; }

    int status() const { return status_; }

private:
    int status_ = 200;
    std::string statusText_ = "OK";
    std::string body_;
    std::map<std::string, std::string> headers_;
    bool isBinary_ = false;
    bool isFileStream_ = false;
    FileStreamInfo fileStreamInfo_;

    static std::string contentTypeForFile(const std::string& path);
};

} // namespace narnat

#endif
