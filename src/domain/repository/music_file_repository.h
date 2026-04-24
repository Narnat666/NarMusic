#ifndef NARNAT_MUSIC_FILE_REPOSITORY_H
#define NARNAT_MUSIC_FILE_REPOSITORY_H

#include "domain/music_metadata.h"
#include <string>
#include <vector>
#include <cstdint>

namespace narnat {

struct MusicFileInfo {
    std::string systemFilename;
    std::string customFilename;
    long long fileSize = 0;
    long long downloadTime = 0;
    int delayMs = 0;
};

class IMusicFileRepository {
public:
    virtual ~IMusicFileRepository() = default;

    // 扫描音乐库
    virtual std::vector<MusicFileInfo> scanLibrary(const std::string& directory) = 0;

    // 文件操作
    virtual bool fileExists(const std::string& path) = 0;
    virtual long long fileSize(const std::string& path) = 0;
    virtual std::string readFile(const std::string& path) = 0;
    virtual bool deleteFile(const std::string& path) = 0;
    virtual bool ensureDirectory(const std::string& path) = 0;

    // M4A元数据写入
    virtual bool writeMetadata(const std::string& filePath, const MusicMetadata& metadata) = 0;
};

} // namespace narnat

#endif
