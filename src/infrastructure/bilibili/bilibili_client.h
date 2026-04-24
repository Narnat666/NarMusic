#ifndef NARNAT_BILIBILI_CLIENT_H
#define NARNAT_BILIBILI_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "infrastructure/http_client/curl_client.h"
#include "domain/music_metadata.h"

namespace narnat {

class BilibiliClient {
public:
    explicit BilibiliClient(std::shared_ptr<CurlClient> httpClient);

    // 从URL提取BVID
    std::string extractBvid(const std::string& url);

    // 获取视频信息（aid, cid）
    bool fetchVideoInfo(const std::string& bvid, std::string& aid, std::string& cid);

    // 获取音频下载URL
    std::string getAudioUrl(const std::string& aid, const std::string& cid);

    // 搜索视频
    std::vector<std::map<std::string, std::string>> search(const std::string& keyword);

    // 从URL提取链接（兼容短链接）
    std::string resolveUrl(const std::string& input);

private:
    // WBI签名相关
    bool initialize();
    std::string fetchAnonymousCookie();
    std::pair<std::string, std::string> fetchWbiKeys();
    std::map<std::string, std::string> signWbiParams(const std::map<std::string, std::string>& params);
    std::string optimizeKeyword(const std::string& raw);
    std::string stripHtmlTags(const std::string& html);

    std::shared_ptr<CurlClient> httpClient_;
    std::string cookie_;
    std::string imgKey_;
    std::string subKey_;
    bool initialized_ = false;
    std::mutex initMutex_;
};

} // namespace narnat

#endif
