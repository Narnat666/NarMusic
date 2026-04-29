#ifndef NARNAT_IAUDIO_DOWNLOADER_H
#define NARNAT_IAUDIO_DOWNLOADER_H

#include <string>
#include <functional>
#include <memory>

namespace narnat {

class IAudioDownloader {
public:
    virtual ~IAudioDownloader() = default;

    virtual std::string download(const std::string& url,
                                 const std::string& filePath,
                                 std::function<void(long long)> progressCallback = nullptr) = 0;
};

} // namespace narnat

#endif
