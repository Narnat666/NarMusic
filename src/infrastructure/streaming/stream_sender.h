#ifndef NARNAT_STREAM_SENDER_H
#define NARNAT_STREAM_SENDER_H

#include <string>
#include <vector>
#include "core/http/response.h"

namespace narnat {

class StreamSender {
public:
    explicit StreamSender(long long bufferSize = 256 * 1024);

    StreamData read(const std::string& filePath, const std::string& rangeHeader);

    FileStreamInfo resolveStreamInfo(const std::string& filePath, const std::string& rangeHeader);

    static long long getFileSize(const std::string& filePath);

private:
    long long bufferSize_;

    bool parseRangeHeader(const std::string& rangeHeader,
                          long long fileSize,
                          long long& start, long long& end);
};

} // namespace narnat

#endif
