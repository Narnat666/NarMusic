#ifndef NARNAT_SQLITE_MUSIC_LIBRARY_REPOSITORY_H
#define NARNAT_SQLITE_MUSIC_LIBRARY_REPOSITORY_H

#include "domain/repository/music_library_repository.h"
#include "infrastructure/persistence/database.h"
#include <memory>

namespace narnat {

class SqliteMusicLibraryRepository : public IMusicLibraryRepository {
public:
    explicit SqliteMusicLibraryRepository(std::shared_ptr<Database> db);

    int save(const MusicLibraryEntry& entry) override;
    std::optional<MusicLibraryEntry> findById(int id) override;
    std::optional<MusicLibraryEntry> findByFilePath(const std::string& filePath) override;
    std::optional<MusicLibraryEntry> findBySystemFilename(const std::string& filename) override;
    std::vector<MusicLibraryEntry> findAll() override;
    void update(const MusicLibraryEntry& entry) override;
    void remove(int id) override;

    void markUsed(int id) override;
    void markUsedByFilename(const std::string& filename) override;

    std::vector<MusicLibraryEntry> findExpired(int maxAgeSeconds) override;
    void removeExpired(int maxAgeSeconds) override;

    void updateSongInfo(int id, const std::string& songName, const std::string& artist, int delayMs) override;

    std::optional<MusicLibraryEntry> findBySongAndArtist(const std::string& songName, const std::string& artist) override;

private:
    void initSchema();

    std::shared_ptr<Database> db_;
};

} // namespace narnat

#endif
