#include "task_manger.h"

TaskManager& TaskManager::instance() { // 返回同一个对象
    static TaskManager manager;
    return manager;
}

std::string TaskManager::generateTaskId() {
    static unsigned counter = 0;          // 同一秒内序号
    std::string ss;
    ss = ss + "task_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(++counter);
    return ss;
}

std::string TaskManager::createTask(const std::string& url) { // 创建线程任务函数
    // 创建任务id
    std::string task_id = generateTaskId();
    auto analyzer = std::make_shared<MusicAnaly>(); // 智能指针接管

    { // 上锁
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task_id] = TaskInfo{task_id, url, analyzer, false, std::chrono::system_clock::now()};
        // 加入到任务，并解锁
    }

    // 启动下载线程
    std::thread([this, task_id, analyzer, url](){
        analyzer->download(url);

        // 等待下载完成
        while (!analyzer->downloadIfFinished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 任务完成
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = tasks_.find(task_id);
            if (it != tasks_.end()) {
                it->second.is_finished = true;
            }
        }
    }).detach();

    return task_id;
}


std::string TaskManager::getTaskStatus(const std::string& task_id) // 状态获取函数
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return R"({"error":"task_not_found"})";
    }

    const auto& info = it->second;
    const auto& analyzer = info.analyzer;
    if (!analyzer) {
        return R"({"error":"analyzer_not_found"})";
    }

    std::string json;
    json.reserve(256);
    json += '{';
    json += "\"task_id\":\"";   json += task_id;                 json += "\",";
    json += "\"url\":\"";       json += info.url;               json += "\",";
    json += "\"is_downloading\":"; json += (analyzer->ifDownloading()         ? "true" : "false"); json += ',';
    json += "\"is_finished\":";   json += (analyzer->downloadIfFinished()    ? "true" : "false"); json += ',';
    json += "\"is_success\":";    json += (analyzer->downloadIfSuccess()     ? "true" : "false"); json += ',';
    json += "\"downloaded_bytes\":"; json += std::to_string(analyzer->getDownloadBytes());
    json += '}';

    return json;
}

// 清理旧任务
void TaskManager::cleanupOldTasks(int max_age_seconds /*=10*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::system_clock::now();

    for (auto it = tasks_.begin(); it != tasks_.end(); ) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
                       now - it->second.created_time);
        if (age.count() > max_age_seconds) {
            std::cout << "erase task: " << it->second.task_id << std::endl;
            it = tasks_.erase(it);
        } else {
            ++it;
        }
    }
}