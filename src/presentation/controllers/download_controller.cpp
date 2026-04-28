#include "download_controller.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"

namespace narnat {

using json = nlohmann::json;

DownloadController::DownloadController(std::shared_ptr<DownloadService> downloadService,
                                        std::shared_ptr<StreamingService> streamingService)
    : downloadService_(std::move(downloadService))
    , streamingService_(std::move(streamingService)) {}

Response DownloadController::createTask(const Request& req) {
    try {
        json body = json::parse(req.body());

        DownloadService::CreateTaskRequest taskReq;
        taskReq.url = body.value("url", "");
        if (taskReq.url.empty()) {
            taskReq.url = body.value("content", "");
        }
        taskReq.filename = body.value("filename", "");
        taskReq.platform = body.value("platform", "网易云音乐");
        taskReq.delayMs = body.value("offsetMs", 0);

        if (taskReq.url.empty()) {
            return Response::error(400, "Bad Request", "missing_url", "URL不能为空");
        }

        std::string taskId = downloadService_->createTask(taskReq);

        json result;
        result["task_id"] = taskId;
        result["message"] = "download_started";
        result["url"] = taskReq.url;
        return Response::json(200, "OK", result);

    } catch (const std::exception& e) {
        LOG_E("DownloadCtrl", std::string("创建任务失败: ") + e.what());
        return Response::error(400, "Bad Request", "parse_error", e.what());
    }
}

Response DownloadController::batchCreateTasks(const Request& req) {
    try {
        json body = json::parse(req.body());

        if (!body.contains("tasks") || !body["tasks"].is_array()) {
            return Response::error(400, "Bad Request", "missing_tasks", "tasks数组参数缺失");
        }

        auto tasksArray = body["tasks"];
        if (tasksArray.empty()) {
            return Response::error(400, "Bad Request", "empty_tasks", "tasks数组不能为空");
        }

        if (tasksArray.size() > 100) {
            return Response::error(400, "Bad Request", "too_many_tasks", "单次批量下载不能超过100个任务");
        }

        std::string defaultPlatform = body.value("platform", "网易云音乐");
        int defaultDelayMs = body.value("offsetMs", 0);

        json results = json::array();
        int successCount = 0;
        int failCount = 0;

        for (const auto& taskObj : tasksArray) {
            DownloadService::CreateTaskRequest taskReq;
            taskReq.url = taskObj.value("url", "");
            if (taskReq.url.empty()) {
                taskReq.url = taskObj.value("content", "");
            }
            taskReq.filename = taskObj.value("filename", "");
            taskReq.platform = taskObj.value("platform", defaultPlatform);
            taskReq.delayMs = taskObj.value("offsetMs", defaultDelayMs);

            json taskResult;
            taskResult["keyword"] = taskObj.value("keyword", "");
            taskResult["title"] = taskObj.value("title", "");
            taskResult["url"] = taskReq.url;

            if (taskReq.url.empty()) {
                taskResult["success"] = false;
                taskResult["error"] = "URL不能为空";
                failCount++;
                results.push_back(taskResult);
                continue;
            }

            try {
                std::string taskId = downloadService_->createTask(taskReq);
                taskResult["success"] = true;
                taskResult["task_id"] = taskId;
                successCount++;
            } catch (const std::exception& e) {
                taskResult["success"] = false;
                taskResult["error"] = e.what();
                failCount++;
            }

            results.push_back(taskResult);
        }

        json result;
        result["results"] = results;
        result["total"] = static_cast<int>(tasksArray.size());
        result["success"] = successCount;
        result["failed"] = failCount;

        return Response::json(200, "OK", result);

    } catch (const std::exception& e) {
        LOG_E("DownloadCtrl", std::string("批量创建任务失败: ") + e.what());
        return Response::error(400, "Bad Request", "parse_error", e.what());
    }
}

Response DownloadController::getStatus(const Request& req) {
    std::string taskId = req.queryParam("task_id");
    if (taskId.empty()) {
        return Response::error(400, "Bad Request", "missing_task_id", "task_id参数缺失");
    }

    auto task = downloadService_->getTaskStatus(taskId);
    if (!task) {
        return Response::error(404, "Not Found", "task_not_found", "任务不存在");
    }

    return Response::json(200, "OK", task->toJson());
}

Response DownloadController::downloadFile(const Request& req) {
    std::string taskId = req.queryParam("task_id");
    std::string filename = req.queryParam("filename");

    std::string id = taskId.empty() ? filename : taskId;
    if (id.empty()) {
        return Response::error(400, "Bad Request", "missing_id", "task_id或filename参数缺失");
    }

    auto fileData = streamingService_->getFileData(id);
    if (fileData.empty()) {
        return Response::error(404, "Not Found", "file_not_found", "文件不存在");
    }

    std::string displayName = streamingService_->getDisplayName(id);
    if (displayName.empty()) {
        displayName = id;
    }
    if (displayName.find('.') == std::string::npos) {
        displayName += ".m4a";
    }

    return Response::download(fileData, displayName);
}

Response DownloadController::stream(const Request& req) {
    std::string taskId = req.queryParam("task_id");
    std::string filename = req.queryParam("filename");

    std::string id = taskId.empty() ? filename : taskId;
    if (id.empty()) {
        return Response::error(400, "Bad Request", "missing_id", "task_id或filename参数缺失");
    }

    auto data = streamingService_->stream(id, req.rangeString());
    if (data.buffer.empty()) {
        return Response::error(404, "Not Found", "file_not_found", "文件不存在");
    }

    return Response::stream(data.buffer, data.fileSize,
                            data.rangeStart, data.rangeEnd, data.isPartial);
}

} // namespace narnat
