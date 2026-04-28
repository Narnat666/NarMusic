#include "sqlite_music_library_repository.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <cstdint>
#include <filesystem>

namespace narnat {

namespace {

const sqlite3_destructor_type kSqliteTransient =
    reinterpret_cast<sqlite3_destructor_type>(static_cast<intptr_t>(-1));

MusicLibraryEntry readRow(sqlite3_stmt* stmt, int baseCol = 0) {
    MusicLibraryEntry entry;
    entry.id = sqlite3_column_int(stmt, baseCol);
    entry.songName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, baseCol + 1));
    entry.artist = reinterpret_cast<const char*>(sqlite3_column_text(stmt, baseCol + 2));
    entry.album = reinterpret_cast<const char*>(sqlite3_column_text(stmt, baseCol + 3));
    entry.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, baseCol + 4));
    entry.systemFilename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, baseCol + 5));
    if (sqlite3_column_type(stmt, baseCol + 6) != SQLITE_NULL) {
        entry.originalFilename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, baseCol + 6));
    }
    entry.fileSize = sqlite3_column_int64(stmt, baseCol + 7);
    entry.delayMs = sqlite3_column_int(stmt, baseCol + 8);
    entry.platform = reinterpret_cast<const char*>(sqlite3_column_text(stmt, baseCol + 9));
    entry.inUse = sqlite3_column_int(stmt, baseCol + 10) != 0;

    int64_t dlAt = sqlite3_column_int64(stmt, baseCol + 11);
    int64_t luAt = sqlite3_column_int64(stmt, baseCol + 12);
    entry.downloadedAt = std::chrono::system_clock::from_time_t(dlAt);
    entry.lastUsedAt = std::chrono::system_clock::from_time_t(luAt);
    return entry;
}

static const char* kSelectColumns =
    "id, song_name, artist, album, file_path, system_filename, original_filename, "
    "file_size, delay_ms, platform, in_use, downloaded_at, last_used_at";

}

SqliteMusicLibraryRepository::SqliteMusicLibraryRepository(std::shared_ptr<Database> db)
    : db_(std::move(db)) {
    initSchema();
}

void SqliteMusicLibraryRepository::initSchema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS music_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            song_name TEXT NOT NULL DEFAULT '',
            artist TEXT NOT NULL DEFAULT '',
            album TEXT NOT NULL DEFAULT '',
            file_path TEXT NOT NULL,
            system_filename TEXT NOT NULL,
            file_size INTEGER NOT NULL DEFAULT 0,
            delay_ms INTEGER NOT NULL DEFAULT 0,
            platform TEXT NOT NULL DEFAULT '',
            in_use INTEGER NOT NULL DEFAULT 0,
            downloaded_at INTEGER NOT NULL,
            last_used_at INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_music_files_downloaded ON music_files(downloaded_at);
        CREATE INDEX IF NOT EXISTS idx_music_files_filename ON music_files(system_filename);
        CREATE INDEX IF NOT EXISTS idx_music_files_dedup ON music_files(song_name, artist, platform, delay_ms);
    )";
    db_->execute(sql);

    sqlite3_stmt* checkStmt = nullptr;
    const char* checkSql = "SELECT platform FROM music_files LIMIT 0";
    bool hasPlatformColumn = (sqlite3_prepare_v2(db_->handle(), checkSql, -1, &checkStmt, nullptr) == SQLITE_OK);
    if (checkStmt) sqlite3_finalize(checkStmt);

    if (!hasPlatformColumn) {
        db_->execute("ALTER TABLE music_files ADD COLUMN platform TEXT NOT NULL DEFAULT ''");
    }

    checkStmt = nullptr;
    checkSql = "SELECT album FROM music_files LIMIT 0";
    bool hasAlbumColumn = (sqlite3_prepare_v2(db_->handle(), checkSql, -1, &checkStmt, nullptr) == SQLITE_OK);
    if (checkStmt) sqlite3_finalize(checkStmt);

    if (!hasAlbumColumn) {
        db_->execute("ALTER TABLE music_files ADD COLUMN album TEXT NOT NULL DEFAULT ''");
    }

    checkStmt = nullptr;
    checkSql = "SELECT original_filename FROM music_files LIMIT 0";
    bool hasOriginalFilenameColumn = (sqlite3_prepare_v2(db_->handle(), checkSql, -1, &checkStmt, nullptr) == SQLITE_OK);
    if (checkStmt) sqlite3_finalize(checkStmt);

    if (!hasOriginalFilenameColumn) {
        db_->execute("ALTER TABLE music_files ADD COLUMN original_filename TEXT DEFAULT NULL");
    }
}

int SqliteMusicLibraryRepository::save(const MusicLibraryEntry& entry) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "INSERT INTO music_files "
        "(song_name, artist, album, file_path, system_filename, original_filename, file_size, delay_ms, platform, in_use, downloaded_at, last_used_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_E("MusicLibRepo", "save prepare失败");
        return 0;
    }

    auto dlSec = std::chrono::system_clock::to_time_t(entry.downloadedAt);
    auto luSec = std::chrono::system_clock::to_time_t(entry.lastUsedAt);

    sqlite3_bind_text(stmt, 1, entry.songName.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 2, entry.artist.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 3, entry.album.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 4, entry.filePath.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 5, entry.systemFilename.c_str(), -1, kSqliteTransient);
    if (entry.originalFilename.empty()) {
        sqlite3_bind_null(stmt, 6);
    } else {
        sqlite3_bind_text(stmt, 6, entry.originalFilename.c_str(), -1, kSqliteTransient);
    }
    sqlite3_bind_int64(stmt, 7, entry.fileSize);
    sqlite3_bind_int(stmt, 8, entry.delayMs);
    sqlite3_bind_text(stmt, 9, entry.platform.c_str(), -1, kSqliteTransient);
    sqlite3_bind_int(stmt, 10, entry.inUse ? 1 : 0);
    sqlite3_bind_int64(stmt, 11, dlSec);
    sqlite3_bind_int64(stmt, 12, luSec);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LOG_E("MusicLibRepo", std::string("save失败: ") + sqlite3_errmsg(db_->handle()));
        sqlite3_finalize(stmt);
        return 0;
    }

    int id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
    sqlite3_finalize(stmt);
    return id;
}

std::optional<MusicLibraryEntry> SqliteMusicLibraryRepository::findById(int id) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM music_files WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_int(stmt, 1, id);

    std::optional<MusicLibraryEntry> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRow(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<MusicLibraryEntry> SqliteMusicLibraryRepository::findByFilePath(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM music_files WHERE file_path = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, kSqliteTransient);

    std::optional<MusicLibraryEntry> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRow(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<MusicLibraryEntry> SqliteMusicLibraryRepository::findBySystemFilename(const std::string& filename) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM music_files WHERE system_filename = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, kSqliteTransient);

    std::optional<MusicLibraryEntry> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRow(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<MusicLibraryEntry> SqliteMusicLibraryRepository::findAll() {
    std::lock_guard<std::mutex> lock(db_->mutex());

    std::string sql = std::string("SELECT ") + kSelectColumns + " FROM music_files ORDER BY downloaded_at DESC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    std::vector<MusicLibraryEntry> entries;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        entries.push_back(readRow(stmt));
    }

    sqlite3_finalize(stmt);
    return entries;
}

void SqliteMusicLibraryRepository::update(const MusicLibraryEntry& entry) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "UPDATE music_files SET song_name=?, artist=?, album=?, file_path=?, "
        "system_filename=?, original_filename=?, file_size=?, delay_ms=?, platform=?, in_use=?, downloaded_at=?, last_used_at=? WHERE id=?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return;

    auto dlSec = std::chrono::system_clock::to_time_t(entry.downloadedAt);
    auto luSec = std::chrono::system_clock::to_time_t(entry.lastUsedAt);

    sqlite3_bind_text(stmt, 1, entry.songName.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 2, entry.artist.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 3, entry.album.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 4, entry.filePath.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 5, entry.systemFilename.c_str(), -1, kSqliteTransient);
    if (entry.originalFilename.empty()) {
        sqlite3_bind_null(stmt, 6);
    } else {
        sqlite3_bind_text(stmt, 6, entry.originalFilename.c_str(), -1, kSqliteTransient);
    }
    sqlite3_bind_int64(stmt, 7, entry.fileSize);
    sqlite3_bind_int(stmt, 8, entry.delayMs);
    sqlite3_bind_text(stmt, 9, entry.platform.c_str(), -1, kSqliteTransient);
    sqlite3_bind_int(stmt, 10, entry.inUse ? 1 : 0);
    sqlite3_bind_int64(stmt, 11, dlSec);
    sqlite3_bind_int64(stmt, 12, luSec);
    sqlite3_bind_int(stmt, 13, entry.id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SqliteMusicLibraryRepository::remove(int id) {
    auto entry = findById(id);
    if (!entry) return;

    int refCount = countByFilePath(entry->filePath);
    if (refCount <= 1 && !entry->filePath.empty()) {
        try {
            std::filesystem::remove(entry->filePath);
            LOG_I("MusicLibRepo", "已删除文件: " + entry->filePath);
        } catch (...) {
            LOG_W("MusicLibRepo", "文件删除失败: " + entry->filePath);
        }
    }

    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "DELETE FROM music_files WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SqliteMusicLibraryRepository::markUsed(int id) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = "UPDATE music_files SET in_use = 1, last_used_at = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SqliteMusicLibraryRepository::markUsedByFilename(const std::string& filename) {
    auto entry = findBySystemFilename(filename);
    if (entry) markUsed(entry->id);
}

std::vector<MusicLibraryEntry> SqliteMusicLibraryRepository::findExpired(int maxAgeSeconds) {
    if (maxAgeSeconds <= 0) return {};

    std::lock_guard<std::mutex> lock(db_->mutex());

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string sql = std::string("SELECT ") + kSelectColumns
        + " FROM music_files WHERE last_used_at < ? AND in_use = 0";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    sqlite3_bind_int64(stmt, 1, now - maxAgeSeconds);

    std::vector<MusicLibraryEntry> entries;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        entries.push_back(readRow(stmt));
    }

    sqlite3_finalize(stmt);
    return entries;
}

void SqliteMusicLibraryRepository::removeExpired(int maxAgeSeconds) {
    auto expired = findExpired(maxAgeSeconds);
    for (auto& entry : expired) {
        remove(entry.id);
        LOG_I("MusicLibRepo", "清理过期文件: " + entry.systemFilename);
    }
}

void SqliteMusicLibraryRepository::updateSongInfo(int id, const std::string& songName,
                                                     const std::string& artist, int delayMs) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "UPDATE music_files SET song_name = ?, artist = ?, delay_ms = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_text(stmt, 1, songName.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 2, artist.c_str(), -1, kSqliteTransient);
    sqlite3_bind_int(stmt, 3, delayMs);
    sqlite3_bind_int(stmt, 4, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::optional<MusicLibraryEntry> SqliteMusicLibraryRepository::findBySongArtistPlatformDelay(
    const std::string& songName, const std::string& artist,
    const std::string& platform, int delayMs) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    std::string sql = std::string("SELECT ") + kSelectColumns
        + " FROM music_files WHERE song_name = ? AND artist = ? AND platform = ? AND delay_ms = ?"
          " ORDER BY downloaded_at DESC LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_text(stmt, 1, songName.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 2, artist.c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 3, platform.c_str(), -1, kSqliteTransient);
    sqlite3_bind_int(stmt, 4, delayMs);

    std::optional<MusicLibraryEntry> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRow(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

int SqliteMusicLibraryRepository::countByFilePath(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "SELECT COUNT(*) FROM music_files WHERE file_path = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, kSqliteTransient);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

} // namespace narnat
