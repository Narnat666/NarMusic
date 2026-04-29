#include "router.h"
#include "core/logger.h"

namespace narnat {

bool Router::hasParams(const std::string& pattern) const {
    return pattern.find(":") != std::string::npos;
}

void Router::addRoute(Request::Method method, const std::string& pattern, Handler handler) {
    Route route;
    route.method = method;
    route.pattern = pattern;
    route.handler = std::move(handler);

    auto segments = splitPath(pattern);
    for (const auto& seg : segments) {
        if (!seg.empty() && seg[0] == ':') {
            route.paramNames.push_back(seg.substr(1));
        }
    }

    if (hasParams(pattern)) {
        dynamicRoutes_.push_back(std::move(route));
    } else {
        RouteKey key{method, pattern};
        staticRoutes_[key] = std::move(route);
    }
}

void Router::addCatchAllRoute(Request::Method method, Handler handler) {
    catchAllMethod_ = method;
    catchAllHandler_ = std::move(handler);
}

void Router::addMiddleware(Middleware mw) {
    middlewares_.push_back(std::move(mw));
}

Response Router::dispatch(const Request& req) {
    for (const auto& mw : middlewares_) {
        auto result = mw(req);
        if (result.has_value()) {
            return result.value();
        }
    }

    RouteKey key{req.method(), req.path()};
    auto it = staticRoutes_.find(key);
    if (it != staticRoutes_.end()) {
        try {
            return it->second.handler(req);
        } catch (const std::exception& e) {
            LOG_E("Router", std::string("Handler异常: ") + e.what());
            return Response::error(500, "Internal Server Error", "handler_error", e.what());
        }
    }

    for (const auto& route : dynamicRoutes_) {
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

    if (catchAllHandler_ && req.method() == catchAllMethod_) {
        try {
            return catchAllHandler_(req);
        } catch (const std::exception& e) {
            LOG_E("Router", std::string("CatchAll Handler异常: ") + e.what());
            return Response::error(500, "Internal Server Error", "handler_error", e.what());
        }
    }

    return Response::error(404, "Not Found", "not_found", "No route matches the request");
}

bool Router::hasMatch(Request::Method method, const std::string& path) const {
    RouteKey key{method, path};
    if (staticRoutes_.find(key) != staticRoutes_.end()) return true;

    for (const auto& route : dynamicRoutes_) {
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
