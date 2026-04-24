#ifndef NARNAT_REQUEST_H
#define NARNAT_REQUEST_H

#include <string>
#include <map>
#include <vector>

namespace narnat {

class Request {
public:
    enum class Method { GET, POST, PUT, DELETE, UNKNOWN };

    Request() = default;

    bool parse(const std::string& raw);

    Method method() const { return method_; }
    std::string methodString() const;
    const std::string& path() const { return path_; }
    const std::string& queryString() const { return queryString_; }
    const std::string& body() const { return body_; }
    int bodyLength() const { return bodyLength_; }

    std::string header(const std::string& key) const;
    const std::map<std::string, std::string>& headers() const { return headers_; }

    // 查询参数解析
    std::string queryParam(const std::string& key) const;

    // 路径参数（由Router填充）
    const std::map<std::string, std::string>& pathParams() const { return pathParams_; }
    void setPathParams(std::map<std::string, std::string>&& params) { pathParams_ = std::move(params); }
    std::string pathParam(const std::string& key) const;

    // Range头
    std::string rangeString() const;

private:
    Method method_ = Method::UNKNOWN;
    std::string path_;
    std::string queryString_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    int bodyLength_ = 0;
    std::map<std::string, std::string> pathParams_;

    static std::string urlDecode(const std::string& str);
};

} // namespace narnat

#endif
