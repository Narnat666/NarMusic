#include "bilibili_client.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"
#include <openssl/md5.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <regex>

namespace narnat {

using json = nlohmann::json;

static const int MIXIN_KEY_ENC_TAB[32] = {
    46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
    27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13
};

static const char* FALLBACK_COOKIE =
    "buvid3=175-9E59-4C12-8A3A-3C76C5B6850865482infoc; "
    "buvid4=6A5E6D2E-1C78-1C42-8F6A-3D7F9C0E5B3265482;";

static std::string md5Hex(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), digest);
    char md5str[33];
    for (int i = 0; i < 16; ++i)
        sprintf(md5str + i * 2, "%02x", digest[i]);
    return std::string(md5str);
}

static std::string urlEncodeWbi(const std::string& s) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }
        escaped << '%' << std::setw(2) << int(c);
    }
    return escaped.str();
}

BilibiliClient::BilibiliClient(std::shared_ptr<CurlClient> httpClient)
    : httpClient_(std::move(httpClient)) {
    initialized_ = initialize();
}

bool BilibiliClient::initialize() {
    cookie_ = fetchAnonymousCookie();
    if (cookie_.empty()) {
        LOG_W("BiliClient", "获取Cookie失败，使用备用Cookie");
        cookie_ = FALLBACK_COOKIE;
    }

    auto keys = fetchWbiKeys();
    imgKey_ = keys.first;
    subKey_ = keys.second;
    if (imgKey_.empty() || subKey_.empty()) {
        LOG_W("BiliClient", "获取WBI密钥失败，使用默认密钥");
        imgKey_ = "7cd084941338484aae1ad9425b84077c";
        subKey_ = "4932caff0ff746eab6f01bf08b70ac45";
    }

    LOG_I("BiliClient", "初始化完成");
    return true;
}

std::string BilibiliClient::fetchAnonymousCookie() {
    // 简化：通过nav接口获取cookie
    auto resp = httpClient_->get("https://api.bilibili.com/x/web-interface/nav",
        {"Referer: https://www.bilibili.com/"});
    // cookie由curl自动处理，这里返回空使用fallback
    return "";
}

std::pair<std::string, std::string> BilibiliClient::fetchWbiKeys() {
    auto resp = httpClient_->get("https://api.bilibili.com/x/web-interface/nav",
        {"Referer: https://www.bilibili.com/", "Cookie: " + cookie_});

    if (!resp.success || resp.body.empty()) return {"", ""};

    try {
        json j = json::parse(resp.body);
        if (j.contains("data") && j["data"].contains("wbi_img")) {
            auto extractKey = [](const std::string& url) -> std::string {
                size_t slash = url.find_last_of('/');
                size_t dot = url.find_last_of('.');
                if (slash != std::string::npos && dot != std::string::npos && dot > slash)
                    return url.substr(slash + 1, dot - slash - 1);
                return "";
            };
            return {
                extractKey(j["data"]["wbi_img"]["img_url"].get<std::string>()),
                extractKey(j["data"]["wbi_img"]["sub_url"].get<std::string>())
            };
        }
    } catch (const std::exception& e) {
        LOG_W("BiliClient", std::string("WBI密钥解析失败: ") + e.what());
    }
    return {"", ""};
}

std::map<std::string, std::string> BilibiliClient::signWbiParams(
    const std::map<std::string, std::string>& params) {
    std::map<std::string, std::string> signedParams = params;
    signedParams["wts"] = std::to_string(time(nullptr));

    std::string rawKey = imgKey_ + subKey_;
    std::string mixinKey;
    for (int i = 0; i < 32; ++i) mixinKey += rawKey[static_cast<size_t>(MIXIN_KEY_ENC_TAB[i])];

    std::string qs;
    for (const auto& p : signedParams) {
        if (p.second.empty()) continue;
        if (!qs.empty()) qs += "&";
        qs += p.first + "=" + urlEncodeWbi(p.second);
    }
    signedParams["w_rid"] = md5Hex(qs + mixinKey);
    return signedParams;
}

std::string BilibiliClient::extractBvid(const std::string& url) {
    size_t pos = url.find("BV");
    if (pos != std::string::npos && pos + 12 <= url.length()) {
        return url.substr(pos, 12);
    }
    return "";
}

bool BilibiliClient::fetchVideoInfo(const std::string& bvid,
                                     std::string& aid, std::string& cid) {
    std::string url = "https://api.bilibili.com/x/web-interface/view?bvid=" + bvid;
    auto resp = httpClient_->get(url, {
        "Referer: https://www.bilibili.com",
        "Origin: https://www.bilibili.com"
    });

    if (!resp.success) {
        LOG_E("BiliClient", "获取视频信息失败: " + bvid);
        return false;
    }

    try {
        json j = json::parse(resp.body);
        if (j.contains("data")) {
            aid = std::to_string(j["data"]["aid"].get<long long>());
            cid = std::to_string(j["data"]["cid"].get<long long>());
            return true;
        }
    } catch (const std::exception& e) {
        LOG_E("BiliClient", std::string("视频信息解析失败: ") + e.what());
    }
    return false;
}

std::string BilibiliClient::getAudioUrl(const std::string& aid, const std::string& cid) {
    std::string url = "https://api.bilibili.com/x/player/playurl?avid=" + aid +
        "&cid=" + cid + "&qn=0&type=&otype=json&fnver=0&fnval=80";

    auto resp = httpClient_->get(url, {
        "Referer: https://www.bilibili.com",
        "Origin: https://www.bilibili.com"
    });

    if (!resp.success) return "";

    try {
        json j = json::parse(resp.body);
        // 递归查找audio.baseUrl
        std::function<std::string(const json&)> findUrl = [&](const json& obj) -> std::string {
            if (obj.is_object()) {
                if (obj.contains("dash") && obj["dash"].contains("audio") &&
                    obj["dash"]["audio"].is_array() && !obj["dash"]["audio"].empty()) {
                    for (auto& item : obj["dash"]["audio"]) {
                        if (item.contains("baseUrl")) return item["baseUrl"].get<std::string>();
                    }
                }
                for (auto& [k, v] : obj.items()) {
                    if (v.is_object() || v.is_array()) {
                        auto r = findUrl(v);
                        if (!r.empty()) return r;
                    }
                }
            } else if (obj.is_array()) {
                for (auto& item : obj) {
                    auto r = findUrl(item);
                    if (!r.empty()) return r;
                }
            }
            return "";
        };
        return findUrl(j);
    } catch (const std::exception& e) {
        LOG_E("BiliClient", std::string("音频URL解析失败: ") + e.what());
    }
    return "";
}

std::string BilibiliClient::optimizeKeyword(const std::string& raw) {
    std::string result = raw;
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    result.erase(std::remove(result.begin(), result.end(), '\t'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '_'), result.end());
    // 移除中间点·和长破折号—
    std::string::size_type pos;
    while ((pos = result.find("\xC2\xB7")) != std::string::npos) result.erase(pos, 2);
    while ((pos = result.find("\xE2\x80\x94")) != std::string::npos) result.erase(pos, 3);
    return result.empty() ? raw : result;
}

std::string BilibiliClient::stripHtmlTags(const std::string& html) {
    return std::regex_replace(html, std::regex("<[^>]*>"), "");
}

std::string BilibiliClient::resolveUrl(const std::string& input) {
    // 提取URL
    size_t start = input.find("http");
    if (start == std::string::npos) return "";
    size_t end = input.find(" ", start);
    std::string url = (end == std::string::npos) ? input.substr(start) : input.substr(start, end - start);

    // 解析短链接
    if (url.find("b23.tv") != std::string::npos) {
        std::string resolved = httpClient_->resolveRedirect(url);
        if (!resolved.empty()) url = resolved;
    }

    return url;
}

std::vector<std::map<std::string, std::string>> BilibiliClient::search(const std::string& keywords) {
    if (!initialized_) {
        LOG_E("BiliClient", "未初始化，无法搜索");
        return {};
    }

    std::vector<std::map<std::string, std::string>> results;

    // 分割关键词
    std::vector<std::string> kws;
    std::stringstream ss(keywords);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        if (!token.empty()) kws.push_back(token);
    }

    for (const auto& kw : kws) {
        std::string optimized = optimizeKeyword(kw);

        std::map<std::string, std::string> params;
        params["keyword"] = optimized;
        params["page"] = "1";
        params["pagesize"] = "20";
        auto signedParams = signWbiParams(params);

        std::string url = "https://api.bilibili.com/x/web-interface/wbi/search/all/v2?";
        for (const auto& p : signedParams) {
            url += p.first + "=" + urlEncodeWbi(p.second) + "&";
        }
        url.pop_back();

        auto resp = httpClient_->get(url, {
            "Referer: https://www.bilibili.com",
            "Cookie: " + cookie_
        });

        std::map<std::string, std::string> item;
        item["name"] = kw;

        if (resp.success && !resp.body.empty()) {
            try {
                json j = json::parse(resp.body);
                if (j.value("code", -1) == 0 && j.contains("data") && j["data"].contains("result")) {
                    // 查找最佳视频
                    std::vector<std::tuple<std::string, std::string, long long>> candidates;
                    for (const auto& resultItem : j["data"]["result"]) {
                        if (resultItem.value("result_type", "") != "video") continue;
                        if (!resultItem.contains("data")) continue;
                        for (const auto& video : resultItem["data"]) {
                            if (!video.contains("duration")) continue;
                            // 时长过滤
                            int dur = 0;
                            if (video["duration"].is_number()) dur = video["duration"].get<int>();
                            if (dur <= 0 || dur > 600) continue;

                            long long play = 0;
                            if (video.contains("play") && video["play"].is_number())
                                play = video["play"].get<long long>();

                            std::string link, title;
                            if (video.contains("bvid") && !video["bvid"].is_null())
                                link = "https://www.bilibili.com/video/" + video["bvid"].get<std::string>();
                            if (video.contains("title") && video["title"].is_string())
                                title = stripHtmlTags(video["title"].get<std::string>());

                            if (!link.empty()) candidates.emplace_back(link, title, play);
                        }
                    }

                    if (!candidates.empty()) {
                        std::sort(candidates.begin(), candidates.end(),
                            [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });
                        item["result"] = "成功";
                        item["link"] = std::get<0>(candidates[0]);
                        item["title"] = std::get<1>(candidates[0]);
                    }
                }
            } catch (const std::exception& e) {
                LOG_W("BiliClient", std::string("搜索解析失败: ") + e.what());
            }
        }

        if (item.find("result") == item.end()) {
            item["result"] = "失败";
            item["link"] = "";
            item["title"] = "";
        }
        results.push_back(item);
    }

    return results;
}

} // namespace narnat
