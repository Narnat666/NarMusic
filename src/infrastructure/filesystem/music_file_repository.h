#ifndef NARNAT_FS_MUSIC_FILE_REPOSITORY_H
#define NARNAT_FS_MUSIC_FILE_REPOSITORY_H

#include "domain/repository/music_file_repository.h"

namespace narnat {

class FsMusicFileRepository : public IMusicFileRepository {
public:
    bool fileExists(const std::string& path) override;
    long long fileSize(const std::string& path) override;
    bool deleteFile(const std::string& path) override;
    bool ensureDirectory(const std::string& path) override;
    bool writeMetadata(const std::string& filePath, const MusicMetadata& metadata) override;
};

} // namespace narnat

#endif
