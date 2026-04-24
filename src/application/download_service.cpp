#include "download_service.h"
#include "core/logger.h"
#include "BS_thread_pool.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace narnat {

static BS::thread_pool<BS::tp::priority> gDownloadPool(5);

DownloadService::DownloadService(std::shared_ptr<ITaskRepository> taskRepo,
                                  std::shared_ptr<IMusicFileRepository> fileRepo,
                                  std::shared_ptr<AudioDownloader> audioDownloader,
                                  std::shared_ptr<LyricsAggregator> lyricsAggregator,
                                  const DownloadConfig& config)
    : taskRepo_(std::move(taskRepo))
    , fileRepo_(std::move(fileRepo))
    , audioDownloader_(std::move(audioDownloader))
    , lyricsAggregator_(std::move(lyricsAggregator))
    , config_(config) {}

std::string DownloadService::createTask(const CreateTaskRequest& req) {
    // 生成任务ID
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    std::string taskId = "task_" + std::to_string(ms) + "_" + std::to_string(taskCounter_++);

    std::string filename = req.filename;
    if (filename.empty()) filename = "music_" + std::to_string(ms);

    std::string filePath = config_.path + filename + config_.extension;

    Task task(taskId, req.url, filePath, filename, req.delayMs);
    task.setStatus(TaskStatus::Pending);
    taskRepo_->save(task);

    LOG_I("DownloadSvc", "创建任务: " + taskId + " url=" + req.url);

    // 提交到线程池异步执行
    std::string taskIdCopy = taskId;
    CreateTaskRequest reqCopy = req;
    gDownloadPool.detach_task([this, taskIdCopy, reqCopy]() {
        executeDownload(taskIdCopy, reqCopy);
    });

    return taskId;
}

void DownloadService::executeDownload(const std::string& taskId,
                                       const CreateTaskRequest& req) {
    auto taskOpt = taskRepo_->findById(taskId);
    if (!taskOpt) return;
    Task task = *taskOpt;

    // 1. 下载音频
    task.setStatus(TaskStatus::Downloading);
    taskRepo_->update(task);

    std::string filePath = audioDownloader_->download(
        req.url, config_.path, req.filename,
        [&task, this](long long bytes) {
            task.setDownloadedBytes(bytes);
            taskRepo_->update(task);
        });

    if (filePath.empty()) {
        task.setStatus(TaskStatus::Failed);
        taskRepo_->update(task);
        LOG_E("DownloadSvc", "下载失败: " + taskId);
        return;
    }

    // 2. 获取歌词和封面
    if (lyricsAggregator_) {
        try {
            auto metadata = lyricsAggregator_->fetchBest(req.filename, req.platform);

            // 应用歌词偏移
            if (req.delayMs != 0 && metadata.hasLyrics) {
                metadata.lyrics = LyricsAggregator::adjustLyricsTiming(metadata.lyrics, req.delayMs);
            }

            // 双语合并
            if (metadata.hasTranslation && !metadata.translationLyrics.empty()) {
                metadata.lyrics = LyricsAggregator::mergeBilingualLyrics(
                    metadata.lyrics, metadata.translationLyrics);
            }

            // 写入M4A元数据
            fileRepo_->writeMetadata(filePath, metadata);
        } catch (const std::exception& e) {
            LOG_W("DownloadSvc", std::string("歌词获取失败: ") + e.what());
        }
    }

    // 3. 标记完成
    task.setStatus(TaskStatus::Finished);
    taskRepo_->update(task);
    LOG_I("DownloadSvc", "任务完成: " + taskId);
}

std::optional<Task> DownloadService::getTaskStatus(const std::string& taskId) {
    return taskRepo_->findById(taskId);
}

std::string DownloadService::getTaskFilePath(const std::string& taskId) {
    auto task = taskRepo_->findById(taskId);
    return task ? task->filePath() : "";
}

std::string DownloadService::getTaskDisplayName(const std::string& taskId) {
    auto task = taskRepo_->findById(taskId);
    return task ? task->displayName() : "";
}

void DownloadService::cleanupExpiredTasks() {
    taskRepo_->removeOlderThan(config_.max_age);
    LOG_D("DownloadSvc", "过期任务已清理");
}

std::optional<Task> DownloadService::findByFilename(const std::string& filename) {
    auto tasks = taskRepo_->findAll();
    for (const auto& task : tasks) {
        if (task.displayName() == filename || task.filePath().find(filename) != std::string::npos) {
            return task;
        }
    }
    return std::nullopt;
}

} // namespace narnat
