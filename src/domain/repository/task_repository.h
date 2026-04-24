#ifndef NARNAT_TASK_REPOSITORY_H
#define NARNAT_TASK_REPOSITORY_H

#include "domain/task.h"
#include <vector>
#include <optional>
#include <string>
#include <memory>

namespace narnat {

class ITaskRepository {
public:
    virtual ~ITaskRepository() = default;

    virtual std::string save(const Task& task) = 0;
    virtual std::optional<Task> findById(const std::string& id) = 0;
    virtual std::vector<Task> findAll() = 0;
    virtual void update(const Task& task) = 0;
    virtual void remove(const std::string& id) = 0;
    virtual void removeOlderThan(int seconds) = 0;
    virtual bool exists(const std::string& id) = 0;
};

} // namespace narnat

#endif
