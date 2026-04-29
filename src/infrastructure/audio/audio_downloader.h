#ifndef NARNAT_AUDIO_DOWNLOADER_H
#define NARNAT_AUDIO_DOWNLOADER_H

#include <string>
#include <memory>
#include <functional>
#include "domain/repository/ibilibili_client.h"
#include "domain/repository/iaudio_downloader.h"

namespace narnat {

class AudioDownloader : public IAudioDownloader {
public:
    explicit AudioDownloader(std::shared_ptr<IBilibiliClient> biliClient);

    std::string download(const std::string& url,
                         const std::string& filePath,
                         std::function<void(long long)> progressCallback = nullptr) override;

private:
    std::shared_ptr<IBilibiliClient> biliClient_;
};

} // namespace narnat

#endif
