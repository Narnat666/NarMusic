#include "bili_searcher.h"
#include <openssl/md5.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <vector>
#include <map>
#include <tuple>
#include <regex>

using json = nlohmann::json;

static const char* CA_BUNDLE_PATH = "/etc/ssl/certs/ca-certificates.crt";
static const char* FALLBACK_COOKIE = "buvid3=175-9E59-4C12-8A3A-3C76C5B6850865482infoc; buvid4=6A5E6D2E-1C78-1C42-8F6A-3D7F9C0E5B3265482;";
static const int MIXIN_KEY_ENC_TAB[32] = {
    46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
    27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    if (!output) return 0;
    size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

static size_t header_callback(char* buffer, size_t size, size_t nitems, std::string* cookies) {
    if (!cookies) return 0;
    size_t total = size * nitems;
    std::string header(buffer, total);
    if (header.find("Set-Cookie:") == 0) {
        size_t pos = header.find("buvid3=");
        if (pos != std::string::npos) {
            size_t end = header.find(";", pos);
            if (end == std::string::npos) end = header.length();
            *cookies += header.substr(pos, end - pos) + "; ";
        }
        pos = header.find("buvid4=");
        if (pos != std::string::npos) {
            size_t end = header.find(";", pos);
            if (end == std::string::npos) end = header.length();
            *cookies += header.substr(pos, end - pos) + "; ";
        }
    }
    return total;
}

static std::string md5_hex(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), digest);
    char md5str[33];
    for (int i = 0; i < 16; ++i)
        sprintf(md5str + i * 2, "%02x", digest[i]);
    return std::string(md5str);
}

static std::string url_encode_wbi(const std::string& s) {
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

BiliSearcher::BiliSearcher()
    : curl_(nullptr), initialized_(false) {
    curl_ = curl_easy_init();
    if (curl_) {
        initialized_ = initialize();
    } else {
        std::cerr << "[BiliSearcher] 初始化CURL失败" << std::endl;
    }
}

BiliSearcher::~BiliSearcher() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

bool BiliSearcher::initialize() {
    curl_easy_setopt(curl_, CURLOPT_URL, "https://www.bilibili.com/");
    curl_easy_setopt(curl_, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cerr << "[BiliSearcher] 网络连接失败: " << curl_easy_strerror(res) << "，将使用硬编码Cookie" << std::endl;
    }
    curl_easy_setopt(curl_, CURLOPT_NOBODY, 0L);

    cookie_ = fetchAnonymousCookie();
    if (cookie_.empty()) {
        std::cerr << "[BiliSearcher] 获取Cookie失败，使用备用Cookie" << std::endl;
        cookie_ = FALLBACK_COOKIE;
    }

    auto keys = fetchWbiKeys();
    img_key_ = keys.first;
    sub_key_ = keys.second;
    if (img_key_.empty() || sub_key_.empty()) {
        std::cerr << "[BiliSearcher] 获取WBI密钥失败，使用默认密钥（可能过期）" << std::endl;
        img_key_ = "7cd084941338484aae1ad9425b84077c";
        sub_key_ = "4932caff0ff746eab6f01bf08b70ac45";
    }

    return true;
}

std::string BiliSearcher::fetchAnonymousCookie() {
    std::string cookie_str;
    std::string dummy_body;

    curl_easy_setopt(curl_, CURLOPT_URL, "https://www.bilibili.com/");
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &dummy_body);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &cookie_str);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 10L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    headers = curl_slist_append(headers, "Referer: https://www.bilibili.com/");
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate, br");
    headers = curl_slist_append(headers, "Connection: keep-alive");
    headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl_, CURLOPT_CAINFO, CA_BUNDLE_PATH);

    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cerr << "[fetchAnonymousCookie] 标准SSL验证失败: " << curl_easy_strerror(res) << "，尝试跳过证书验证..." << std::endl;
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
        res = curl_easy_perform(curl_);
    }

    curl_slist_free_all(headers);

    if (cookie_str.empty()) {
        std::cerr << "[fetchAnonymousCookie] 主页获取Cookie失败，尝试 nav 接口..." << std::endl;
        dummy_body.clear();
        cookie_str.clear();
        curl_easy_setopt(curl_, CURLOPT_URL, "https://api.bilibili.com/x/web-interface/nav");
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &dummy_body);
        curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &cookie_str);
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, nullptr);
        headers = curl_slist_append(nullptr, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl_);
        curl_slist_free_all(headers);
    }

    return cookie_str;
}

std::string BiliSearcher::httpGet(const std::string& url) {
    std::string response;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);

    struct curl_slist* headers = nullptr;
    if (!cookie_.empty()) {
        headers = curl_slist_append(headers, ("Cookie: " + cookie_).c_str());
    }
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    headers = curl_slist_append(headers, "Referer: https://www.bilibili.com/");
    headers = curl_slist_append(headers, "Accept: application/json, text/plain, */*");
    headers = curl_slist_append(headers, "Accept-Language: zh-CN,zh;q=0.9");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cerr << "[httpGet] HTTP请求失败: " << curl_easy_strerror(res) << std::endl;
        response.clear();
    }

    curl_slist_free_all(headers);
    return response;
}

std::pair<std::string, std::string> BiliSearcher::fetchWbiKeys() {
    std::string img_key, sub_key;
    std::string response = httpGet("https://api.bilibili.com/x/web-interface/nav");

    if (!response.empty()) {
        try {
            json j = json::parse(response);
            if (j.contains("data") && j["data"].contains("wbi_img")) {
                std::string img_url = j["data"]["wbi_img"]["img_url"].get<std::string>();
                std::string sub_url = j["data"]["wbi_img"]["sub_url"].get<std::string>();

                size_t last_slash = img_url.find_last_of('/');
                size_t last_dot = img_url.find_last_of('.');
                if (last_slash != std::string::npos && last_dot != std::string::npos && last_dot > last_slash) {
                    img_key = img_url.substr(last_slash + 1, last_dot - last_slash - 1);
                }

                last_slash = sub_url.find_last_of('/');
                last_dot = sub_url.find_last_of('.');
                if (last_slash != std::string::npos && last_dot != std::string::npos && last_dot > last_slash) {
                    sub_key = sub_url.substr(last_slash + 1, last_dot - last_slash - 1);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[fetchWbiKeys] JSON解析失败: " << e.what() << std::endl;
        }
    }

    return {img_key, sub_key};
}

std::map<std::string, std::string> BiliSearcher::signWbiParams(const std::map<std::string, std::string>& params) {
    std::map<std::string, std::string> signed_params = params;
    time_t now = time(nullptr);
    signed_params["wts"] = std::to_string(now);

    std::string raw_key = img_key_ + sub_key_;
    std::string mixin_key;
    for (int i = 0; i < 32; ++i) {
        mixin_key += raw_key[MIXIN_KEY_ENC_TAB[i]];
    }

    std::string query_string;
    for (const auto& p : signed_params) {
        if (p.second.empty()) continue;
        if (!query_string.empty()) query_string += "&";
        query_string += p.first + "=" + url_encode_wbi(p.second);
    }

    signed_params["w_rid"] = md5_hex(query_string + mixin_key);
    return signed_params;
}

std::string BiliSearcher::optimizeKeyword(const std::string& raw) {
    std::string result = raw;

    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    result.erase(std::remove(result.begin(), result.end(), '\t'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '_'), result.end());

    std::string::size_type pos;
    while ((pos = result.find("\xC2\xB7")) != std::string::npos) {
        result.erase(pos, 2);
    }

    while ((pos = result.find("\xE2\x80\x94")) != std::string::npos) {
        result.erase(pos, 3);
    }

    if (result.empty()) {
        return raw;
    }
    return result;
}

std::string BiliSearcher::stripHtmlTags(const std::string& html) {
    return std::regex_replace(html, std::regex("<[^>]*>"), "");
}

SearchResult BiliSearcher::searchSingle(const std::string& keyword) {
    SearchResult empty_result{"", ""};
    std::cout << "[搜索] " << keyword << std::endl;

    std::map<std::string, std::string> params;
    params["keyword"] = keyword;
    params["page"] = "1";
    params["pagesize"] = "20";

    auto signed_params = signWbiParams(params);

    std::string url = "https://api.bilibili.com/x/web-interface/wbi/search/all/v2?";
    for (const auto& p : signed_params) {
        url += p.first + "=" + url_encode_wbi(p.second) + "&";
    }
    url.pop_back();

    std::string response = httpGet(url);
    if (response.empty()) {
        std::cerr << "[searchSingle] 搜索请求失败" << std::endl;
        return empty_result;
    }

    auto extractVideoLink = [](const json& video) -> std::string {
        if (video.contains("bvid") && !video["bvid"].is_null()) {
            std::string bvid = video["bvid"].get<std::string>();
            if (!bvid.empty()) {
                return "https://www.bilibili.com/video/" + bvid;
            }
        }
        if (video.contains("arcurl") && !video["arcurl"].is_null()) {
            std::string link = video["arcurl"].get<std::string>();
            if (!link.empty()) {
                if (link.find("http") == std::string::npos) link = "https:" + link;
                return link;
            }
        }
        if (video.contains("aid") && !video["aid"].is_null()) {
            if (video["aid"].is_number()) {
                long long aid = video["aid"].get<long long>();
                return "https://www.bilibili.com/video/av" + std::to_string(aid);
            } else if (video["aid"].is_string()) {
                std::string aid = video["aid"].get<std::string>();
                if (!aid.empty()) return "https://www.bilibili.com/video/av" + aid;
            }
        }
        return "";
    };

    auto parseDuration = [](const json& dur_field) -> int {
        if (dur_field.is_number()) return dur_field.get<int>();
        if (dur_field.is_string()) {
            std::string dur_str = dur_field.get<std::string>();
            if (std::all_of(dur_str.begin(), dur_str.end(), ::isdigit)) {
                try { return std::stoi(dur_str); } catch (...) {}
            }
            std::vector<std::string> parts;
            std::stringstream ss(dur_str);
            std::string part;
            while (std::getline(ss, part, ':')) parts.push_back(part);
            try {
                if (parts.size() == 2) return std::stoi(parts[0]) * 60 + std::stoi(parts[1]);
                if (parts.size() == 3) return std::stoi(parts[0]) * 3600 + std::stoi(parts[1]) * 60 + std::stoi(parts[2]);
            } catch (...) {}
            std::vector<int> nums;
            std::string num_buf;
            for (char c : dur_str) {
                if (isdigit(c)) num_buf += c;
                else {
                    if (!num_buf.empty()) { nums.push_back(std::stoi(num_buf)); num_buf.clear(); }
                }
            }
            if (!num_buf.empty()) nums.push_back(std::stoi(num_buf));
            if (nums.size() == 1) {
                if (dur_str.find("秒") != std::string::npos && dur_str.find("分") == std::string::npos) return nums[0];
                else return nums[0] * 60;
            } else if (nums.size() >= 2) {
                return nums[0] * 60 + nums[1];
            }
        }
        return 0;
    };

    try {
        json j = json::parse(response);
        int code = j.value("code", -1);
        if (code != 0) {
            std::cerr << "[API错误] code=" << code << " msg=" << j.value("message", "") << std::endl;
            return empty_result;
        }
        if (!j.contains("data") || !j["data"].contains("result")) {
            std::cerr << "[API错误] 缺少result字段" << std::endl;
            return empty_result;
        }

        std::vector<std::tuple<std::string, std::string, long long, int>> candidates;

        for (const auto& result_item : j["data"]["result"]) {
            if (result_item.value("result_type", "") != "video") continue;
            if (!result_item.contains("data")) continue;
            for (const auto& video : result_item["data"]) {
                if (!video.contains("duration")) continue;
                int seconds = parseDuration(video["duration"]);
                if (seconds > 0 && seconds <= 600) {
                    long long play = 0;
                    if (video.contains("play") && !video["play"].is_null()) {
                        if (video["play"].is_number()) {
                            play = video["play"].get<long long>();
                        } else if (video["play"].is_string()) {
                            std::string play_str = video["play"].get<std::string>();
                            try { play = std::stoll(play_str); } catch (...) {}
                        }
                    }
                    std::string link = extractVideoLink(video);
                    std::string title;
                    if (video.contains("title") && video["title"].is_string()) {
                        title = stripHtmlTags(video["title"].get<std::string>());
                    }
                    if (!link.empty()) {
                        candidates.emplace_back(link, title, play, seconds);
                    }
                }
            }
        }

        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end(),
                [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });
            SearchResult result;
            result.link = std::get<0>(candidates[0]);
            result.title = std::get<1>(candidates[0]);
            std::cerr << "[结果] 时长: " << std::get<3>(candidates[0]) << "秒 播放量: "
                      << std::get<2>(candidates[0]) << " 标题: " << result.title << " 链接: " << result.link << std::endl;
            return result;
        } else {
            std::cerr << "[搜索] 未找到≤10分钟的视频" << std::endl;
        }

    } catch (const json::exception& e) {
        std::cerr << "[JSON解析失败] " << e.what() << std::endl;
        std::cerr << "响应原文(前300): " << response.substr(0, 300) << std::endl;
    }

    return empty_result;
}

SearchResult BiliSearcher::searchWithFallback(const std::string& raw_keyword) {
    std::string optimized = optimizeKeyword(raw_keyword);
    SearchResult result = searchSingle(optimized);
    if (!result.link.empty()) {
        return result;
    }
    std::cerr << "[降级] 优化词失败，尝试原始关键词: " << raw_keyword << std::endl;
    return searchSingle(raw_keyword);
}

std::vector<std::map<std::string, std::string>> BiliSearcher::search(const std::string& keywords_input) {
    std::vector<std::map<std::string, std::string>> results;

    if (!initialized_) {
        std::cerr << "[BiliSearcher] 未正确初始化，无法执行搜索" << std::endl;
        return results;
    }

    std::vector<std::string> raw_keywords;
    std::stringstream ss(keywords_input);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        if (!token.empty()) {
            raw_keywords.push_back(token);
        }
    }

    if (raw_keywords.empty()) {
        std::cerr << "[BiliSearcher] 关键词不能为空" << std::endl;
        return results;
    }

    for (const auto& kw : raw_keywords) {
        std::map<std::string, std::string> item;
        item["name"] = kw;
        SearchResult result = searchWithFallback(kw);
        if (!result.link.empty()) {
            item["result"] = "成功";
            item["link"] = result.link;
            item["title"] = result.title;
        } else {
            item["result"] = "失败";
            item["link"] = "";
            item["title"] = "";
        }
        results.push_back(item);
    }

    return results;
}
