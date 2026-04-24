#ifndef NARNAT_CURL_CLIENT_H
#define NARNAT_CURL_CLIENT_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>

namespace narnat {

struct HttpResponse {
    long statusCode = 0;
    std::string body;
    std::vector<uint8_t> binaryBody;
    bool success = false;
    std::string error;
};

class CurlClient {
public:
    struct Options {
        long connectTimeout = 10;
        long requestTimeout = 15;
        bool followRedirects = true;
        bool verifySsl = false;
        std::string userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    };

    CurlClient();
    explicit CurlClient(const Options& opts);

    // GET请求
    HttpResponse get(const std::string& url,
                     const std::vector<std::string>& headers = {});

    // POST请求
    HttpResponse post(const std::string& url,
                      const std::string& data,
                      const std::vector<std::string>& headers = {});

    // 下载文件（二进制）
    HttpResponse download(const std::string& url,
                          const std::vector<std::string>& headers = {});

    // 下载文件到磁盘
    bool downloadToFile(const std::string& url,
                        const std::string& filePath,
                        const std::vector<std::string>& headers = {},
                        std::function<void(long long)> progressCallback = nullptr);

    // URL编码
    std::string urlEncode(const std::string& value);

    // 解析短链接重定向
    std::string resolveRedirect(const std::string& url);

private:
    Options opts_;
    std::mutex mutex_;  // curl非线程安全，需串行化
};

} // namespace narnat

#endif
