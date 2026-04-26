#ifndef NARNAT_MUSIC_LIBRARY_REPOSITORY_H
#define NARNAT_MUSIC_LIBRARY_REPOSITORY_H

#include "domain/music_library_entry.h"
#include <vector>
#include <string>
#include <optional>

namespace narnat {

class IMusicLibraryRepository {
public:
    virtual ~IMusicLibraryRepository() = default;

    virtual int save(const MusicLibraryEntry& entry) = 0;
    virtual std::optional<MusicLibraryEntry> findById(int id) = 0;
    virtual std::optional<MusicLibraryEntry> findByFilePath(const std::string& filePath) = 0;
    virtual std::optional<MusicLibraryEntry> findBySystemFilename(const std::string& filename) = 0;
    virtual std::vector<MusicLibraryEntry> findAll() = 0;
    virtual void update(const MusicLibraryEntry& entry) = 0;
    virtual void remove(int id) = 0;

    virtual void markUsed(int id) = 0;
    virtual void markUsedByFilename(const std::string& filename) = 0;

    virtual std::vector<MusicLibraryEntry> findExpired(int maxAgeSeconds) = 0;
    virtual void removeExpired(int maxAgeSeconds) = 0;

    virtual void updateSongInfo(int id, const std::string& songName, const std::string& artist, int delayMs) = 0;
};

} // namespace narnat

#endif
