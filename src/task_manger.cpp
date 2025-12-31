#include "task_manger.h"
#include "nlohmann/json.hpp"
#include <filesystem>

using json = nlohmann::json;

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

std::string TaskManager::createTask(const std::string& url, const std::string& file_name) { // 创建线程任务函数
    // 创建任务id
    std::string task_id = generateTaskId();
    std::string name = task_id;
    if (!file_name.empty()) name = file_name; // 文件名为空则用taskid做为名字

    auto analyzer = std::make_shared<MusicAnaly>(name); // 智能指针接管
    { // 上锁
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task_id] = TaskInfo{task_id, url, analyzer, false, std::chrono::system_clock::now(), analyzer->getDownloadFilePathName()};
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

    // 使用nlohmann/json创建JSON
    json j;
    j["task_id"] = task_id;
    j["url"] = info.url;
    j["is_downloading"] = analyzer->ifDownloading();
    j["is_finished"] = analyzer->downloadIfFinished();
    j["is_success"] = analyzer->downloadIfSuccess();
    j["downloaded_bytes"] = analyzer->getDownloadBytes();

    return j.dump();
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
            std::string file_path_name = it->second.file_path_name;
            try {
                // 检查文件是否存在
                if (!std::filesystem::exists(file_path_name)) {
                    std::cout << "文件不存在: " << file_path_name << std::endl;
                    return;
                }
                
                // 删除文件
                bool success = std::filesystem::remove(file_path_name);
                
                if (success) {
                    std::cout << "已删除: " << file_path_name << std::endl;
                } else {
                    std::cerr << "删除失败: " << file_path_name << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "删除异常: " << e.what() << " 文件: " << file_path_name << std::endl;
            }
            it = tasks_.erase(it);
        } else {
            ++it;
        }
    }
}