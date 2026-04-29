#ifndef NARNAT_SEARCH_SERVICE_H
#define NARNAT_SEARCH_SERVICE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "domain/repository/ibilibili_client.h"

namespace narnat {

class SearchService {
public:
    explicit SearchService(std::shared_ptr<IBilibiliClient> biliClient);

    std::vector<std::map<std::string, std::string>> search(const std::string& keyword);

private:
    std::shared_ptr<IBilibiliClient> biliClient_;
};

} // namespace narnat

#endif
