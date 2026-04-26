#include "search_controller.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"
#include "threadpool/BS_thread_pool.hpp"

namespace narnat {

using json = nlohmann::json;

SearchController::SearchController(std::shared_ptr<SearchService> searchService)
    : searchService_(std::move(searchService)) {}

Response SearchController::search(const Request& req) {
    std::string keyword = req.queryParam("keyword");
    if (keyword.empty()) {
        return Response::error(400, "Bad Request", "missing_keyword", "keyword参数缺失");
    }

    auto results = searchService_->search(keyword);

    json result;
    result["keyword"] = keyword;

    if (!results.empty()) {
        const auto& first = results[0];
        result["link"] = first.count("link") ? first.at("link") : "";
        result["title"] = first.count("title") ? first.at("title") : "";
    } else {
        result["link"] = "";
        result["title"] = "";
    }

    return Response::json(200, "OK", result);
}

Response SearchController::batchSearch(const Request& req) {
    try {
        json body = json::parse(req.body());

        if (!body.contains("keywords") || !body["keywords"].is_array()) {
            return Response::error(400, "Bad Request", "missing_keywords", "keywords数组参数缺失");
        }

        auto keywords = body["keywords"].get<std::vector<std::string>>();
        if (keywords.empty()) {
            return Response::error(400, "Bad Request", "empty_keywords", "keywords数组不能为空");
        }

        if (keywords.size() > 100) {
            return Response::error(400, "Bad Request", "too_many_keywords", "单次批量搜索不能超过100个关键词");
        }

        struct BatchSearchResult {
            std::string keyword;
            std::string link;
            std::string title;
            bool found;
        };

        std::vector<BatchSearchResult> searchResults(keywords.size());
        BS::thread_pool<BS::tp::priority> pool(std::min(static_cast<size_t>(4), keywords.size()));

        std::vector<std::future<void>> futures;
        futures.reserve(keywords.size());

        for (size_t i = 0; i < keywords.size(); ++i) {
            futures.push_back(pool.submit_task([this, &keywords, &searchResults, i]() {
                const auto& keyword = keywords[i];
                auto results = searchService_->search(keyword);

                searchResults[i].keyword = keyword;
                if (!results.empty()) {
                    const auto& first = results[0];
                    searchResults[i].link = first.count("link") ? first.at("link") : "";
                    searchResults[i].title = first.count("title") ? first.at("title") : "";
                    searchResults[i].found = !searchResults[i].link.empty();
                } else {
                    searchResults[i].link = "";
                    searchResults[i].title = "";
                    searchResults[i].found = false;
                }
            }));
        }

        for (auto& f : futures) {
            f.wait();
        }

        json resultArray = json::array();
        int foundCount = 0;
        int notFoundCount = 0;

        for (const auto& sr : searchResults) {
            json item;
            item["keyword"] = sr.keyword;
            item["link"] = sr.link;
            item["title"] = sr.title;
            item["found"] = sr.found;
            resultArray.push_back(item);

            if (sr.found) {
                foundCount++;
            } else {
                notFoundCount++;
            }
        }

        json result;
        result["results"] = resultArray;
        result["total"] = static_cast<int>(keywords.size());
        result["found"] = foundCount;
        result["not_found"] = notFoundCount;

        return Response::json(200, "OK", result);

    } catch (const std::exception& e) {
        LOG_E("SearchCtrl", std::string("批量搜索失败: ") + e.what());
        return Response::error(400, "Bad Request", "parse_error", e.what());
    }
}

} // namespace narnat
