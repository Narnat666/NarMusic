#include "audio_downloader.h"
#include "core/logger.h"
#include <filesystem>

namespace narnat {

AudioDownloader::AudioDownloader(std::shared_ptr<BilibiliClient> biliClient)
    : biliClient_(std::move(biliClient)) {}

std::string AudioDownloader::download(const std::string& url,
                                       const std::string& saveDir,
                                       const std::string& filename,
                                       std::function<void(long long)> progressCallback) {
    // 1. 解析URL
    std::string resolvedUrl = biliClient_->resolveUrl(url);
    std::string bvid = biliClient_->extractBvid(resolvedUrl);

    if (bvid.empty()) {
        LOG_E("AudioDL", "无法提取BVID: " + url);
        return "";
    }

    // 2. 获取视频信息
    std::string aid, cid;
    if (!biliClient_->fetchVideoInfo(bvid, aid, cid)) {
        LOG_E("AudioDL", "获取视频信息失败: " + bvid);
        return "";
    }

    // 3. 获取音频URL
    std::string audioUrl = biliClient_->getAudioUrl(aid, cid);
    if (audioUrl.empty()) {
        LOG_E("AudioDL", "获取音频URL失败");
        return "";
    }

    // 4. 下载音频文件
    namespace fs = std::filesystem;
    fs::create_directories(saveDir);

    std::string filePath = saveDir + filename + ".m4a";

    // 直接使用CurlClient下载
    CurlClient::Options opts;
    opts.connectTimeout = 10;
    opts.requestTimeout = 120;
    CurlClient client(opts);

    if (!client.downloadToFile(audioUrl, filePath, {}, progressCallback)) {
        LOG_E("AudioDL", "音频下载失败: " + audioUrl);
        return "";
    }

    LOG_I("AudioDL", "下载完成: " + filePath);
    return filePath;
}

} // namespace narnat
