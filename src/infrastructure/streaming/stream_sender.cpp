#include "stream_sender.h"
#include "core/logger.h"
#include <fstream>
#include <regex>

namespace narnat {

StreamSender::StreamSender(long long bufferSize) : bufferSize_(bufferSize) {}

long long StreamSender::getFileSize(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) return -1;
    return file.tellg();
}

bool StreamSender::parseRangeHeader(const std::string& rangeHeader,
                                     long long fileSize,
                                     long long& start, long long& end) {
    if (rangeHeader.empty() || fileSize <= 0) {
        start = 0;
        end = fileSize - 1;
        return false;
    }

    std::regex rangeRegex("bytes=(\\d+)-(\\d*)", std::regex_constants::icase);
    std::smatch match;

    if (std::regex_search(rangeHeader, match, rangeRegex) && match.size() >= 3) {
        try {
            start = std::stoll(match[1].str());
            std::string endStr = match[2].str();
            end = endStr.empty() ? fileSize - 1 : std::stoll(endStr);

            if (start < 0 || start >= fileSize) {
                LOG_W("StreamSender", "无效起始位置: " + std::to_string(start));
                return false;
            }
            if (end >= fileSize) end = fileSize - 1;
            if (start > end) return false;

            return true;
        } catch (const std::exception& e) {
            LOG_W("StreamSender", std::string("Range解析失败: ") + e.what());
            return false;
        }
    }

    start = 0;
    end = fileSize - 1;
    return false;
}

StreamData StreamSender::read(const std::string& filePath, const std::string& rangeHeader) {
    StreamData result;
    result.fileSize = getFileSize(filePath);

    if (result.fileSize <= 0) {
        LOG_W("StreamSender", "文件不存在或为空: " + filePath);
        return result;
    }

    result.isPartial = parseRangeHeader(rangeHeader, result.fileSize,
                                         result.rangeStart, result.rangeEnd);
    result.requestSize = result.rangeEnd - result.rangeStart + 1;

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        LOG_E("StreamSender", "文件打开失败: " + filePath);
        return result;
    }

    file.seekg(result.rangeStart, std::ios::beg);
    result.buffer.resize(static_cast<size_t>(result.requestSize));
    if (!file.read(result.buffer.data(), result.requestSize)) {
        LOG_E("StreamSender", "文件读取失败");
        return result;
    }

    return result;
}

FileStreamInfo StreamSender::resolveStreamInfo(const std::string& filePath,
                                                const std::string& rangeHeader) {
    FileStreamInfo info;
    info.filePath = filePath;
    info.fileSize = getFileSize(filePath);

    if (info.fileSize <= 0) {
        LOG_W("StreamSender", "文件不存在或为空: " + filePath);
        return info;
    }

    long long start = 0, end = info.fileSize - 1;
    info.isPartial = parseRangeHeader(rangeHeader, info.fileSize, start, end);
    info.rangeStart = start;
    info.rangeEnd = end;

    return info;
}

} // namespace narnat
