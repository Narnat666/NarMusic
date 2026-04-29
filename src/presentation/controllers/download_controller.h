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

    Response createTask(const Request& req);

    Response batchCreateTasks(const Request& req);

    Response getStatus(const Request& req);

    Response batchGetStatus(const Request& req);

    Response downloadFile(const Request& req);

    Response stream(const Request& req);

private:
    std::shared_ptr<DownloadService> downloadService_;
    std::shared_ptr<StreamingService> streamingService_;
};

} // namespace narnat

#endif
