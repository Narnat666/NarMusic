#ifndef NARNAT_MUSIC_FILE_REPOSITORY_H
#define NARNAT_MUSIC_FILE_REPOSITORY_H

#include "domain/music_metadata.h"
#include <string>
#include <cstdint>

namespace narnat {

class IMusicFileRepository {
public:
    virtual ~IMusicFileRepository() = default;

    virtual bool fileExists(const std::string& path) = 0;
    virtual long long fileSize(const std::string& path) = 0;
    virtual bool deleteFile(const std::string& path) = 0;
    virtual bool ensureDirectory(const std::string& path) = 0;

    virtual bool writeMetadata(const std::string& filePath, const MusicMetadata& metadata) = 0;
};

} // namespace narnat

#endif
