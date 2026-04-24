#ifndef NARNAT_SEARCH_CONTROLLER_H
#define NARNAT_SEARCH_CONTROLLER_H

#include <memory>
#include "core/http/request.h"
#include "core/http/response.h"
#include "application/search_service.h"

namespace narnat {

class SearchController {
public:
    explicit SearchController(std::shared_ptr<SearchService> searchService);

    // GET /api/search - 搜索B站视频
    Response search(const Request& req);

private:
    std::shared_ptr<SearchService> searchService_;
};

} // namespace narnat

#endif
