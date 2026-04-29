#ifndef NARNAT_STREAMING_SERVICE_H
#define NARNAT_STREAMING_SERVICE_H

#include <string>
#include <memory>
#include "infrastructure/streaming/stream_sender.h"
#include "domain/repository/task_repository.h"
#include "domain/repository/music_library_repository.h"
#include "config/config.h"

namespace narnat {

class StreamingService {
public:
    StreamingService(std::shared_ptr<ITaskRepository> taskRepo,
                     std::shared_ptr<IMusicLibraryRepository> libraryRepo,
                     const DownloadConfig& config);

    StreamData stream(const std::string& taskIdOrFilename,
                      const std::string& rangeHeader);

    FileStreamInfo streamFileInfo(const std::string& taskIdOrFilename,
                                  const std::string& rangeHeader);

    std::vector<char> getFileData(const std::string& taskIdOrFilename);

    std::string getFilePath(const std::string& taskIdOrFilename);

    std::string getDisplayName(const std::string& taskIdOrFilename);

private:
    std::shared_ptr<ITaskRepository> taskRepo_;
    std::shared_ptr<IMusicLibraryRepository> libraryRepo_;
    StreamSender streamSender_;
    DownloadConfig config_;
};

} // namespace narnat

#endif
