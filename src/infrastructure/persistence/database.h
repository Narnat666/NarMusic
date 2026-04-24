#ifndef NARNAT_DATABASE_H
#define NARNAT_DATABASE_H

#include <string>
#include <memory>
#include <mutex>

struct sqlite3;

namespace narnat {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() { return db_; }
    std::mutex& mutex() { return mutex_; }

    bool execute(const std::string& sql);
    bool isOpen() const { return db_ != nullptr; }

private:
    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};

} // namespace narnat

#endif
