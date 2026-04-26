#ifndef NARNAT_DOWNLOAD_SERVICE_H
#define NARNAT_DOWNLOAD_SERVICE_H

#include <string>
#include <memory>
#include "domain/task.h"
#include "domain/repository/task_repository.h"
#include "domain/repository/music_library_repository.h"
#include "domain/repository/music_file_repository.h"
#include "infrastructure/audio/audio_downloader.h"
#include "infrastructure/lyrics/lyrics_aggregator.h"
#include "config/config.h"

namespace narnat {

class DownloadService {
public:
    struct CreateTaskRequest {
        std::string url;
        std::string filename;
        std::string platform;
        int delayMs = 0;
    };

    DownloadService(std::shared_ptr<ITaskRepository> taskRepo,
                    std::shared_ptr<IMusicLibraryRepository> libraryRepo,
                    std::shared_ptr<IMusicFileRepository> fileRepo,
                    std::shared_ptr<AudioDownloader> audioDownloader,
                    std::shared_ptr<LyricsAggregator> lyricsAggregator,
                    const DownloadConfig& config);

    std::string createTask(const CreateTaskRequest& req);

    std::optional<Task> getTaskStatus(const std::string& taskId);
    std::string getTaskFilePath(const std::string& taskId);
    std::string getTaskDisplayName(const std::string& taskId);

    void cleanupExpiredTasks();

    std::optional<Task> findByFilename(const std::string& filename);

private:
    void executeDownload(const std::string& taskId, const CreateTaskRequest& req);

    std::shared_ptr<ITaskRepository> taskRepo_;
    std::shared_ptr<IMusicLibraryRepository> libraryRepo_;
    std::shared_ptr<IMusicFileRepository> fileRepo_;
    std::shared_ptr<AudioDownloader> audioDownloader_;
    std::shared_ptr<LyricsAggregator> lyricsAggregator_;
    DownloadConfig config_;
    std::atomic<int> taskCounter_{0};
};

} // namespace narnat

#endif
