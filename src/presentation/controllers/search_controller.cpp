#include "search_controller.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"

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

} // namespace narnat
