#ifndef NARNAT_LIBRARY_CONTROLLER_H
#define NARNAT_LIBRARY_CONTROLLER_H

#include <memory>
#include "core/http/request.h"
#include "core/http/response.h"
#include "application/library_service.h"

namespace narnat {

class LibraryController {
public:
    explicit LibraryController(std::shared_ptr<LibraryService> libraryService);

    // GET /api/library/list - 获取音乐库列表
    Response list(const Request& req);

private:
    std::shared_ptr<LibraryService> libraryService_;
};

} // namespace narnat

#endif
