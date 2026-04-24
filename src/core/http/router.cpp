#include "router.h"
#include "core/logger.h"

namespace narnat {

void Router::addRoute(Request::Method method, const std::string& pattern, Handler handler) {
    Route route;
    route.method = method;
    route.pattern = pattern;
    route.handler = std::move(handler);

    // 提取路径参数名（如 :task_id）
    auto segments = splitPath(pattern);
    for (const auto& seg : segments) {
        if (!seg.empty() && seg[0] == ':') {
            route.paramNames.push_back(seg.substr(1));
        }
    }

    routes_.push_back(std::move(route));
}

Response Router::dispatch(const Request& req) {
    for (const auto& route : routes_) {
        if (route.method != req.method()) continue;

        auto [matched, params] = matchPath(route.pattern, req.path());
        if (matched) {
            Request mutableReq = req;
            mutableReq.setPathParams(std::move(params));
            try {
                return route.handler(mutableReq);
            } catch (const std::exception& e) {
                LOG_E("Router", std::string("Handler异常: ") + e.what());
                return Response::error(500, "Internal Server Error", "handler_error", e.what());
            }
        }
    }

    return Response::error(404, "Not Found", "not_found", "No route matches the request");
}

bool Router::hasMatch(Request::Method method, const std::string& path) const {
    for (const auto& route : routes_) {
        if (route.method == method) {
            auto [matched, _] = matchPath(route.pattern, path);
            if (matched) return true;
        }
    }
    return false;
}

std::pair<bool, std::map<std::string, std::string>> Router::matchPath(
    const std::string& pattern, const std::string& path) const {

    auto patternSegs = splitPath(pattern);
    auto pathSegs = splitPath(path);

    if (patternSegs.size() != pathSegs.size()) return {false, {}};

    std::map<std::string, std::string> params;
    for (size_t i = 0; i < patternSegs.size(); ++i) {
        if (!patternSegs[i].empty() && patternSegs[i][0] == ':') {
            // 路径参数
            params[patternSegs[i].substr(1)] = pathSegs[i];
        } else if (patternSegs[i] != pathSegs[i]) {
            return {false, {}};
        }
    }

    return {true, params};
}

std::vector<std::string> Router::splitPath(const std::string& path) const {
    std::vector<std::string> segments;
    size_t start = 0;
    if (!path.empty() && path[0] == '/') start = 1;

    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) {
            segments.push_back(path.substr(start));
            break;
        }
        segments.push_back(path.substr(start, end - start));
        start = end + 1;
    }

    return segments;
}

} // namespace narnat
