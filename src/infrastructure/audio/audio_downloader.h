#ifndef NARNAT_AUDIO_DOWNLOADER_H
#define NARNAT_AUDIO_DOWNLOADER_H

#include <string>
#include <memory>
#include <functional>
#include "infrastructure/bilibili/bilibili_client.h"

namespace narnat {

class AudioDownloader {
public:
    explicit AudioDownloader(std::shared_ptr<BilibiliClient> biliClient);

    // 从B站URL下载音频到指定路径
    // 返回下载的文件路径，失败返回空
    std::string download(const std::string& url,
                         const std::string& filePath,
                         std::function<void(long long)> progressCallback = nullptr);

private:
    std::shared_ptr<BilibiliClient> biliClient_;
};

} // namespace narnat

#endif
