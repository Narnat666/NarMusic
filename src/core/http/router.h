#ifndef NARNAT_ROUTER_H
#define NARNAT_ROUTER_H

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include "request.h"
#include "response.h"

namespace narnat {

using Handler = std::function<Response(const Request&)>;

class Router {
public:
    // 注册路由
    void addRoute(Request::Method method, const std::string& pattern, Handler handler);

    // 分发请求
    Response dispatch(const Request& req);

    // 检查是否有匹配的路由
    bool hasMatch(Request::Method method, const std::string& path) const;

private:
    struct Route {
        Request::Method method;
        std::string pattern;          // 如 "/api/download/status"
        std::vector<std::string> paramNames;  // 路径参数名
        Handler handler;
    };

    std::vector<Route> routes_;

    // 路径模式匹配，返回是否匹配及路径参数
    std::pair<bool, std::map<std::string, std::string>> matchPath(
        const std::string& pattern, const std::string& path) const;

    // 分解路径为段
    std::vector<std::string> splitPath(const std::string& path) const;
};

} // namespace narnat

#endif
