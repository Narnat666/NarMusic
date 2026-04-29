#include "sqlite_task_repository.h"
#include "sqlite_common.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <sstream>
#include <cstdint>

namespace narnat {

SqliteTaskRepository::SqliteTaskRepository(std::shared_ptr<Database> db)
    : db_(std::move(db)) {
    initSchema();
}

void SqliteTaskRepository::initSchema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id TEXT PRIMARY KEY,
            url TEXT NOT NULL,
            status INTEGER NOT NULL DEFAULT 0,
            downloaded_bytes INTEGER NOT NULL DEFAULT 0,
            file_path TEXT NOT NULL,
            display_name TEXT NOT NULL,
            delay_ms INTEGER NOT NULL DEFAULT 0,
            in_use INTEGER NOT NULL DEFAULT 0,
            created_at INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_tasks_created ON tasks(created_at);
    )";
    db_->execute(sql);
}

std::string SqliteTaskRepository::save(const Task& task) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "INSERT OR REPLACE INTO tasks "
        "(id, url, status, downloaded_bytes, file_path, display_name, delay_ms, in_use, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_E("TaskRepo", "prepare失败");
        return task.id();
    }

    auto createdSec = std::chrono::duration_cast<std::chrono::seconds>(
        task.createdAt().time_since_epoch()).count();

    sqlite3_bind_text(stmt, 1, task.id().c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 2, task.url().c_str(), -1, kSqliteTransient);
    sqlite3_bind_int(stmt, 3, static_cast<int>(task.status()));
    sqlite3_bind_int64(stmt, 4, task.downloadedBytes());
    sqlite3_bind_text(stmt, 5, task.filePath().c_str(), -1, kSqliteTransient);
    sqlite3_bind_text(stmt, 6, task.displayName().c_str(), -1, kSqliteTransient);
    sqlite3_bind_int(stmt, 7, task.delayMs());
    sqlite3_bind_int(stmt, 8, task.inUse() ? 1 : 0);
    sqlite3_bind_int64(stmt, 9, createdSec);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LOG_E("TaskRepo", std::string("插入失败: ") + sqlite3_errmsg(db_->handle()));
    }
    sqlite3_finalize(stmt);
    return task.id();
}

std::optional<Task> SqliteTaskRepository::findById(const std::string& id) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "SELECT id, url, status, downloaded_bytes, file_path, "
        "display_name, delay_ms, in_use, created_at FROM tasks WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, kSqliteTransient);

    std::optional<Task> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Task task;
        task = Task(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)),
            sqlite3_column_int(stmt, 6)
        );
        task.setStatus(static_cast<TaskStatus>(sqlite3_column_int(stmt, 2)));
        task.setDownloadedBytes(sqlite3_column_int64(stmt, 3));
        task.setInUse(sqlite3_column_int(stmt, 7) != 0);
        result = std::move(task);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<Task> SqliteTaskRepository::findAll() {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "SELECT id, url, status, downloaded_bytes, file_path, "
        "display_name, delay_ms, in_use, created_at FROM tasks ORDER BY created_at DESC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }

    std::vector<Task> tasks;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Task task(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)),
            sqlite3_column_int(stmt, 6)
        );
        task.setStatus(static_cast<TaskStatus>(sqlite3_column_int(stmt, 2)));
        task.setDownloadedBytes(sqlite3_column_int64(stmt, 3));
        task.setInUse(sqlite3_column_int(stmt, 7) != 0);
        tasks.push_back(std::move(task));
    }

    sqlite3_finalize(stmt);
    return tasks;
}

void SqliteTaskRepository::update(const Task& task) {
    save(task);
}

void SqliteTaskRepository::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(db_->mutex());

    const char* sql = "DELETE FROM tasks WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, kSqliteTransient);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SqliteTaskRepository::removeOlderThan(int seconds) {
    if (seconds <= 0) return;

    std::lock_guard<std::mutex> lock(db_->mutex());

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = "DELETE FROM tasks WHERE created_at < ? AND in_use = 0";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, now - seconds);
    sqlite3_step(stmt);

    const char* markSql = "UPDATE tasks SET in_use = 0 WHERE created_at < ? AND in_use = 1";
    if (sqlite3_prepare_v2(db_->handle(), markSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, now - seconds);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
}

bool SqliteTaskRepository::exists(const std::string& id) {
    return findById(id).has_value();
}

} // namespace narnat
