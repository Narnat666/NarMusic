#ifndef NARNAT_DOWNLOAD_CONTROLLER_H
#define NARNAT_DOWNLOAD_CONTROLLER_H

#include <memory>
#include "core/http/request.h"
#include "core/http/response.h"
#include "application/download_service.h"
#include "application/streaming_service.h"

namespace narnat {

class DownloadController {
public:
    DownloadController(std::shared_ptr<DownloadService> downloadService,
                       std::shared_ptr<StreamingService> streamingService);

    // POST /api/message - 创建下载任务
    Response createTask(const Request& req);

    // GET /api/download/status - 查询任务状态
    Response getStatus(const Request& req);

    // GET /api/download/file - 下载完整文件
    Response downloadFile(const Request& req);

    // GET /api/download/stream - 流式播放
    Response stream(const Request& req);

private:
    std::shared_ptr<DownloadService> downloadService_;
    std::shared_ptr<StreamingService> streamingService_;
};

} // namespace narnat

#endif
