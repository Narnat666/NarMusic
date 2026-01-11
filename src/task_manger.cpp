#include "task_manger.h"
#include "nlohmann/json.hpp"
#include <filesystem>

using json = nlohmann::json;

extern std::string path;
extern std::string ext;

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

    auto analyzer = std::make_shared<MusicAnaly>(task_id, ext, path); // 智能指针接管
    { // 上锁
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task_id] = TaskInfo{task_id, url, analyzer, false, std::chrono::system_clock::now(), analyzer->getDownloadFilePathName(), name + analyzer->getDownloadFileType(), false};
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

void TaskManager::cleanupOldTasks(int max_age_seconds /*=10*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::system_clock::now();

    for (auto it = tasks_.begin(); it != tasks_.end(); ) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
                       now - it->second.created_time);
        
        // 任务还没到删除时间，跳过
        if (age.count() <= max_age_seconds) {
            ++it;
            continue;
        }
        
        // 获取当前任务的信息
        std::string file_path_name = it->second.file_path_name;
        std::string task_id = it->second.task_id;
        
        // 检查文件是否正在使用
        if (it->second.ifusing) {
            // 标记为下次删除
            it->second.ifusing = false;
            std::cout << "文件：" << file_path_name 
                      << " (任务ID: " << task_id 
                      << ") 正在使用中，标记为下次删除" << std::endl;
            ++it;
            continue;
        }
        
        // 删除文件
        try {
            if (std::filesystem::exists(file_path_name)) {
                bool success = std::filesystem::remove(file_path_name);
                if (success) {
                    std::cout << "文件已删除: " << file_path_name << std::endl;
                } else {
                    std::cerr << "文件删除失败: " << file_path_name << std::endl;
                }
            } else {
                std::cout << "文件不存在，无需删除: " << file_path_name << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "文件删除异常: " << e.what() 
                      << " 文件: " << file_path_name << std::endl;
        }
        
        // 从任务列表中移除
        std::cout << "任务已清理: " << task_id << std::endl;
        it = tasks_.erase(it);
    }
}