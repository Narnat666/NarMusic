#ifndef BILISEARCHER_H
#define BILISEARCHER_H

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <curl/curl.h>

struct SearchResult {
    std::string title;
    std::string link;
};

class BiliSearcher {
public:
    BiliSearcher();
    ~BiliSearcher();

    BiliSearcher(const BiliSearcher&) = delete;
    BiliSearcher& operator=(const BiliSearcher&) = delete;

    std::vector<std::map<std::string, std::string>> search(const std::string& keywords_input);

private:
    CURL* curl_;
    std::string cookie_;
    std::string img_key_;
    std::string sub_key_;
    bool initialized_;

    bool initialize();
    std::string fetchAnonymousCookie();
    std::string httpGet(const std::string& url);
    std::pair<std::string, std::string> fetchWbiKeys();
    std::map<std::string, std::string> signWbiParams(const std::map<std::string, std::string>& params);
    std::string optimizeKeyword(const std::string& raw);
    SearchResult searchSingle(const std::string& keyword);
    SearchResult searchWithFallback(const std::string& raw_keyword);
    std::string stripHtmlTags(const std::string& html);
};

#endif
