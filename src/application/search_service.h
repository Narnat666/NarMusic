#ifndef NARNAT_SEARCH_SERVICE_H
#define NARNAT_SEARCH_SERVICE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "infrastructure/bilibili/bilibili_client.h"

namespace narnat {

class SearchService {
public:
    explicit SearchService(std::shared_ptr<BilibiliClient> biliClient);

    std::vector<std::map<std::string, std::string>> search(const std::string& keyword);

private:
    std::shared_ptr<BilibiliClient> biliClient_;
};

} // namespace narnat

#endif
