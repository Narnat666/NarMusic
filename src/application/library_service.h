#ifndef NARNAT_LIBRARY_SERVICE_H
#define NARNAT_LIBRARY_SERVICE_H

#include <string>
#include <vector>
#include <memory>
#include "nlohmann/json.hpp"
#include "domain/repository/music_file_repository.h"
#include "config/config.h"

namespace narnat {

class LibraryService {
public:
    LibraryService(std::shared_ptr<IMusicFileRepository> fileRepo,
                   const DownloadConfig& config);

    nlohmann::json listFiles();

private:
    std::shared_ptr<IMusicFileRepository> fileRepo_;
    DownloadConfig config_;
};

} // namespace narnat

#endif
