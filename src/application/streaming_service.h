#ifndef NARNAT_STREAMING_SERVICE_H
#define NARNAT_STREAMING_SERVICE_H

#include <string>
#include <memory>
#include "infrastructure/streaming/stream_sender.h"
#include "domain/repository/task_repository.h"
#include "config/config.h"

namespace narnat {

class StreamingService {
public:
    StreamingService(std::shared_ptr<ITaskRepository> taskRepo,
                     const DownloadConfig& config);

    // 获取流式音频数据
    StreamData stream(const std::string& taskIdOrFilename,
                      const std::string& rangeHeader);

    // 获取完整文件数据
    std::vector<char> getFileData(const std::string& taskIdOrFilename);

    // 获取文件路径
    std::string getFilePath(const std::string& taskIdOrFilename);

private:
    std::shared_ptr<ITaskRepository> taskRepo_;
    StreamSender streamSender_;
    DownloadConfig config_;
};

} // namespace narnat

#endif
