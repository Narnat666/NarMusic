#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

// 用于创建线程与前端通信，支持多并发

#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include "music_analysis.h"
#include "BS_thread_pool.hpp"

// 下载任务结构体
struct TaskInfo { 
    std::string task_id;  // 线程标志，区分同一时间同一用户的不同下载任务
    std::string url; // 链接
    std::shared_ptr<MusicAnaly> analyzer; // 下载类
    bool is_finished = false;
    std::chrono::system_clock::time_point created_time; // 时间
    std::string file_path_name; // 存储真实路径和名称
    std::string file_send_name; // 文件发送名称
    bool ifusing; // 查看是否正在使用
    int delay_ms = 0; // 延迟设置（毫秒）
};

class TaskManager { 
    public:
        static TaskManager& instance(); // 全局返还全局变量manager
        std::string createTask(const std::string& url, const std::string& file_name, 
                                const std::string& platform, int offsetMs); // 创建任务函数
        std::string getTaskStatus(const std::string& task_id); // 获取任务状态
        void cleanupOldTasks(int max_age_seconds = 10); // 清理函数
        std::map<std::string, TaskInfo> tasks_; // map
        std::mutex mutex_;
        
    private:
        std::string generateTaskId(); // 创建taskid
        BS::thread_pool<BS::tp::priority> pool_{5};
};


#endif // TASK_MANAGER_H