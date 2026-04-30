#ifndef NARNAT_CURL_CLIENT_H
#define NARNAT_CURL_CLIENT_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>
#include <deque>

namespace narnat {

struct HttpResponse {
    long statusCode = 0;
    std::string body;
    std::vector<uint8_t> binaryBody;
    std::vector<std::string> headers;
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

    HttpResponse get(const std::string& url,
                     const std::vector<std::string>& headers = {});

    HttpResponse post(const std::string& url,
                      const std::string& data,
                      const std::vector<std::string>& headers = {});

    HttpResponse download(const std::string& url,
                          const std::vector<std::string>& headers = {});

    bool downloadToFile(const std::string& url,
                        const std::string& filePath,
                        const std::vector<std::string>& headers = {},
                        std::function<void(long long)> progressCallback = nullptr);

    std::string urlEncode(const std::string& value);

    std::string resolveRedirect(const std::string& url);

private:
    struct CurlHandle {
        void* easy = nullptr;
        void* headers = nullptr;
    };

    CurlHandle acquireHandle(const std::vector<std::string>& headers,
                             bool setUserAgent = true);
    void releaseHandle(CurlHandle&& handle);

    void applyCommonOptions(void* curl);

    Options opts_;
    std::mutex poolMutex_;
    std::deque<void*> handlePool_;
};

} // namespace narnat

#endif
