#ifndef NARNAT_LIBRARY_SERVICE_H
#define NARNAT_LIBRARY_SERVICE_H

#include <memory>
#include "nlohmann/json.hpp"
#include "domain/repository/music_library_repository.h"

namespace narnat {

class LibraryService {
public:
    explicit LibraryService(std::shared_ptr<IMusicLibraryRepository> libraryRepo);

    nlohmann::json listFiles();
    bool deleteFile(int id);

private:
    std::shared_ptr<IMusicLibraryRepository> libraryRepo_;
};

} // namespace narnat

#endif
