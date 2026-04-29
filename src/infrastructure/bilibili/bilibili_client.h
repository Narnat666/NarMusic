#ifndef NARNAT_BILIBILI_CLIENT_H
#define NARNAT_BILIBILI_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "infrastructure/http_client/curl_client.h"
#include "domain/music_metadata.h"
#include "domain/repository/ibilibili_client.h"

namespace narnat {

class BilibiliClient : public IBilibiliClient {
public:
    explicit BilibiliClient(std::shared_ptr<CurlClient> httpClient);

    std::string extractBvid(const std::string& url) override;

    bool fetchVideoInfo(const std::string& bvid, std::string& aid, std::string& cid) override;

    std::string getAudioUrl(const std::string& aid, const std::string& cid) override;

    std::vector<std::map<std::string, std::string>> search(const std::string& keyword) override;

    std::string resolveUrl(const std::string& input) override;

private:
    bool initialize();
    std::string fetchAnonymousCookie();
    std::pair<std::string, std::string> fetchWbiKeys();
    std::map<std::string, std::string> signWbiParams(const std::map<std::string, std::string>& params);
    std::string optimizeKeyword(const std::string& raw);
    std::string stripHtmlTags(const std::string& html);
    std::map<std::string, std::string> searchSingle(const std::string& keyword);

    std::shared_ptr<CurlClient> httpClient_;
    std::string cookie_;
    std::string imgKey_;
    std::string subKey_;
    bool initialized_ = false;
    std::mutex initMutex_;
};

} // namespace narnat

#endif
