#ifndef NARNAT_STREAM_SENDER_H
#define NARNAT_STREAM_SENDER_H

#include <string>
#include <vector>

namespace narnat {

struct StreamData {
    std::vector<char> buffer;
    long long fileSize = 0;
    long long rangeStart = 0;
    long long rangeEnd = 0;
    long long requestSize = 0;
    bool isPartial = false;
};

class StreamSender {
public:
    explicit StreamSender(long long bufferSize = 288 * 1024);

    // 解析Range头并读取对应数据
    StreamData read(const std::string& filePath, const std::string& rangeHeader);

    // 获取文件大小
    static long long getFileSize(const std::string& filePath);

private:
    long long bufferSize_;

    bool parseRangeHeader(const std::string& rangeHeader,
                          long long fileSize,
                          long long& start, long long& end);
};

} // namespace narnat

#endif
