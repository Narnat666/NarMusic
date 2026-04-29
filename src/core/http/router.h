#ifndef NARNAT_ROUTER_H
#define NARNAT_ROUTER_H

#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include "request.h"
#include "response.h"
#include "core/rate_limiter.h"

namespace narnat {

using Handler = std::function<Response(const Request&)>;

using Middleware = std::function<std::optional<Response>(const Request&)>;

class Router {
public:
    void addRoute(Request::Method method, const std::string& pattern, Handler handler);

    void addCatchAllRoute(Request::Method method, Handler handler);

    void addMiddleware(Middleware mw);

    Response dispatch(const Request& req);

    bool hasMatch(Request::Method method, const std::string& path) const;

private:
    struct Route {
        Request::Method method;
        std::string pattern;
        std::vector<std::string> paramNames;
        Handler handler;
    };

    struct RouteKey {
        Request::Method method;
        std::string path;

        bool operator==(const RouteKey& other) const {
            return method == other.method && path == other.path;
        }
    };

    struct RouteKeyHash {
        size_t operator()(const RouteKey& k) const {
            size_t h1 = std::hash<int>{}(static_cast<int>(k.method));
            size_t h2 = std::hash<std::string>{}(k.path);
            return h1 ^ (h2 << 1);
        }
    };

    std::unordered_map<RouteKey, Route, RouteKeyHash> staticRoutes_;
    std::vector<Route> dynamicRoutes_;
    Handler catchAllHandler_;
    Request::Method catchAllMethod_ = Request::Method::GET;
    std::vector<Middleware> middlewares_;

    std::pair<bool, std::map<std::string, std::string>> matchPath(
        const std::string& pattern, const std::string& path) const;

    std::vector<std::string> splitPath(const std::string& path) const;

    bool hasParams(const std::string& pattern) const;
};

} // namespace narnat

#endif
