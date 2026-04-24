#ifndef NARNAT_STATIC_FILE_HANDLER_H
#define NARNAT_STATIC_FILE_HANDLER_H

#include <string>
#include "core/http/request.h"
#include "core/http/response.h"

namespace narnat {

class StaticFileHandler {
public:
    explicit StaticFileHandler(const std::string& webDir);

    // 处理静态文件请求
    Response handle(const Request& req);

    // 判断是否为静态文件请求
    bool isStaticFileRequest(const std::string& path) const;

private:
    std::string webDir_;

    std::string contentTypeForPath(const std::string& path) const;
};

} // namespace narnat

#endif
