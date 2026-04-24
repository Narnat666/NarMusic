#include "library_controller.h"

namespace narnat {

LibraryController::LibraryController(std::shared_ptr<LibraryService> libraryService)
    : libraryService_(std::move(libraryService)) {}

Response LibraryController::list(const Request& /*req*/) {
    auto files = libraryService_->listFiles();
    return Response::json(200, "OK", files);
}

} // namespace narnat
