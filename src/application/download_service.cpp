#include "download_service.h"
#include "core/logger.h"
#include "BS_thread_pool.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace narnat {

static BS::thread_pool<BS::tp::priority> gDownloadPool(5);

DownloadService::DownloadService(std::shared_ptr<ITaskRepository> taskRepo,
                                  std::shared_ptr<IMusicLibraryRepository> libraryRepo,
                                  std::shared_ptr<IMusicFileRepository> fileRepo,
                                  std::shared_ptr<AudioDownloader> audioDownloader,
                                  std::shared_ptr<LyricsAggregator> lyricsAggregator,
                                  const DownloadConfig& config)
    : taskRepo_(std::move(taskRepo))
    , libraryRepo_(std::move(libraryRepo))
    , fileRepo_(std::move(fileRepo))
    , audioDownloader_(std::move(audioDownloader))
    , lyricsAggregator_(std::move(lyricsAggregator))
    , config_(config) {}

std::string DownloadService::createTask(const CreateTaskRequest& req) {
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

    MusicMetadata metadata;
    metadata.songName = req.filename;
    metadata.delayMs = req.delayMs;

    std::string delayStr = (req.delayMs >= 0 ? "+" : "") + std::to_string(req.delayMs);
    metadata.narmeta = req.url + "|" + req.platform + "|" + delayStr + "|" + req.filename;

    if (lyricsAggregator_) {
        try {
            auto fetched = lyricsAggregator_->fetchBest(req.filename, req.platform);

            if (req.delayMs != 0 && fetched.hasLyrics) {
                fetched.lyrics = LyricsAggregator::adjustLyricsTiming(fetched.lyrics, req.delayMs);
            }

            if (req.delayMs != 0 && fetched.hasTranslation && !fetched.translationLyrics.empty()) {
                fetched.translationLyrics = LyricsAggregator::adjustLyricsTiming(fetched.translationLyrics, req.delayMs);
            }

            if (fetched.hasTranslation && !fetched.translationLyrics.empty()) {
                fetched.lyrics = LyricsAggregator::mergeBilingualLyrics(
                    fetched.lyrics, fetched.translationLyrics);
            }

            if (!fetched.songName.empty()) metadata.songName = fetched.songName;
            if (!fetched.artist.empty()) metadata.artist = fetched.artist;
            if (!fetched.album.empty()) metadata.album = fetched.album;
            if (fetched.hasLyrics) metadata.lyrics = fetched.lyrics;
            if (fetched.hasCover) metadata.coverData = std::move(fetched.coverData);
            metadata.hasLyrics = fetched.hasLyrics;
            metadata.hasCover = fetched.hasCover;
        } catch (const std::exception& e) {
            LOG_W("DownloadSvc", std::string("歌词获取失败: ") + e.what());
        }
    }

    fileRepo_->writeMetadata(filePath, metadata);

    namespace fs = std::filesystem;
    MusicLibraryEntry entry;
    entry.songName = metadata.songName;
    entry.artist = metadata.artist;
    entry.album = metadata.album;
    entry.filePath = filePath;
    entry.systemFilename = fs::path(filePath).filename().string();
    try { entry.fileSize = static_cast<int64_t>(fs::file_size(filePath)); } catch (...) { entry.fileSize = 0; }
    entry.delayMs = req.delayMs;
    entry.inUse = false;
    entry.downloadedAt = std::chrono::system_clock::now();
    entry.lastUsedAt = entry.downloadedAt;

    int libId = libraryRepo_->save(entry);
    LOG_I("DownloadSvc", "已注册到音乐库: id=" + std::to_string(libId) + " file=" + entry.systemFilename);

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
    libraryRepo_->removeExpired(config_.max_age);
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
