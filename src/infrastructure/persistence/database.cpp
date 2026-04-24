#include "database.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>

namespace narnat {

Database::Database(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_E("Database", std::string("打开数据库失败: ") + sqlite3_errmsg(db_));
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }

    // 启用WAL模式提升并发性能
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA synchronous=NORMAL");

    LOG_I("Database", "数据库已打开: " + path);
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::execute(const std::string& sql) {
    if (!db_) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        LOG_E("Database", std::string("SQL执行失败: ") + (errMsg ? errMsg : "unknown"));
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

} // namespace narnat
