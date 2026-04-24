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

    json result = json::array();
    for (const auto& item : results) {
        json j;
        for (const auto& [k, v] : item) {
            j[k] = v;
        }
        result.push_back(j);
    }

    return Response::json(200, "OK", result);
}

} // namespace narnat
