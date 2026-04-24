#ifndef NARNAT_SQLITE_TASK_REPOSITORY_H
#define NARNAT_SQLITE_TASK_REPOSITORY_H

#include "domain/repository/task_repository.h"
#include "infrastructure/persistence/database.h"
#include <memory>

namespace narnat {

class SqliteTaskRepository : public ITaskRepository {
public:
    explicit SqliteTaskRepository(std::shared_ptr<Database> db);

    std::string save(const Task& task) override;
    std::optional<Task> findById(const std::string& id) override;
    std::vector<Task> findAll() override;
    void update(const Task& task) override;
    void remove(const std::string& id) override;
    void removeOlderThan(int seconds) override;
    bool exists(const std::string& id) override;

private:
    void initSchema();

    std::shared_ptr<Database> db_;
};

} // namespace narnat

#endif
