#include "search_service.h"
#include "core/logger.h"

namespace narnat {

SearchService::SearchService(std::shared_ptr<IBilibiliClient> biliClient)
    : biliClient_(std::move(biliClient)) {}

std::vector<std::map<std::string, std::string>> SearchService::search(const std::string& keyword) {
    LOG_I("SearchSvc", "搜索: " + keyword);
    return biliClient_->search(keyword);
}

} // namespace narnat
