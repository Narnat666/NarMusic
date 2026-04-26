#ifndef NARNAT_LIBRARY_SERVICE_H
#define NARNAT_LIBRARY_SERVICE_H

#include <memory>
#include <vector>
#include "nlohmann/json.hpp"
#include "domain/repository/music_library_repository.h"

namespace narnat {

class LibraryService {
public:
    explicit LibraryService(std::shared_ptr<IMusicLibraryRepository> libraryRepo);

    nlohmann::json listFiles();
    bool deleteFile(int id);
    bool deleteFiles(const std::vector<int>& ids);
    std::vector<std::pair<std::string, std::vector<char>>> getFilesData(const std::vector<int>& ids);

private:
    std::shared_ptr<IMusicLibraryRepository> libraryRepo_;
};

} // namespace narnat

#endif
