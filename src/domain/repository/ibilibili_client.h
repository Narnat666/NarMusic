#ifndef NARNAT_IBILIBILI_CLIENT_H
#define NARNAT_IBILIBILI_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace narnat {

class IBilibiliClient {
public:
    virtual ~IBilibiliClient() = default;

    virtual std::string extractBvid(const std::string& url) = 0;

    virtual bool fetchVideoInfo(const std::string& bvid, std::string& aid, std::string& cid) = 0;

    virtual std::string getAudioUrl(const std::string& aid, const std::string& cid) = 0;

    virtual std::vector<std::map<std::string, std::string>> search(const std::string& keyword) = 0;

    virtual std::string resolveUrl(const std::string& input) = 0;
};

} // namespace narnat

#endif
