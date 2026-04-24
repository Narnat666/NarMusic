#include "curl_client.h"
#include "core/logger.h"
#include <curl/curl.h>
#include <fstream>

namespace narnat {

// 回调函数
static size_t writeStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

static size_t writeBinaryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* buf = static_cast<std::vector<uint8_t>*>(userp);
    const auto* data = static_cast<const uint8_t*>(contents);
    buf->insert(buf->end(), data, data + total);
    return total;
}

static size_t writeFileCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    return fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
}

static int curlProgressCallback(void* clientp, curl_off_t /*dltotal*/, curl_off_t dlnow,
                                curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* cb = static_cast<std::function<void(long long)>*>(clientp);
    if (cb && *cb && dlnow > 0) {
        (*cb)(dlnow);
    }
    return 0;
}

CurlClient::CurlClient() : opts_() {}

CurlClient::CurlClient(const Options& opts) : opts_(opts) {}

HttpResponse CurlClient::get(const std::string& url,
                              const std::vector<std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);

    CURL* curl = curl_easy_init();
    HttpResponse resp;
    if (!curl) { resp.error = "curl init failed"; return resp; }

    struct curl_slist* headerList = nullptr;
    for (const auto& h : headers) headerList = curl_slist_append(headerList, h.c_str());
    if (!opts_.userAgent.empty())
        headerList = curl_slist_append(headerList, ("User-Agent: " + opts_.userAgent).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, opts_.requestTimeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, opts_.connectTimeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, opts_.followRedirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, opts_.verifySsl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, opts_.verifySsl ? 2L : 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.statusCode);
        resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);
    } else {
        resp.error = curl_easy_strerror(res);
        LOG_W("CurlClient", "GET失败: " + resp.error + " url=" + url);
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return resp;
}

HttpResponse CurlClient::post(const std::string& url,
                               const std::string& data,
                               const std::vector<std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);

    CURL* curl = curl_easy_init();
    HttpResponse resp;
    if (!curl) { resp.error = "curl init failed"; return resp; }

    struct curl_slist* headerList = nullptr;
    for (const auto& h : headers) headerList = curl_slist_append(headerList, h.c_str());
    if (!opts_.userAgent.empty())
        headerList = curl_slist_append(headerList, ("User-Agent: " + opts_.userAgent).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, opts_.requestTimeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, opts_.connectTimeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, opts_.followRedirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, opts_.verifySsl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, opts_.verifySsl ? 2L : 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.statusCode);
        resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);
    } else {
        resp.error = curl_easy_strerror(res);
        LOG_W("CurlClient", "POST失败: " + resp.error);
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return resp;
}

HttpResponse CurlClient::download(const std::string& url,
                                   const std::vector<std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);

    CURL* curl = curl_easy_init();
    HttpResponse resp;
    if (!curl) { resp.error = "curl init failed"; return resp; }

    struct curl_slist* headerList = nullptr;
    for (const auto& h : headers) headerList = curl_slist_append(headerList, h.c_str());
    if (!opts_.userAgent.empty())
        headerList = curl_slist_append(headerList, ("User-Agent: " + opts_.userAgent).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBinaryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.binaryBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, opts_.requestTimeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.statusCode);
        resp.success = true;
    } else {
        resp.error = curl_easy_strerror(res);
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return resp;
}

bool CurlClient::downloadToFile(const std::string& url,
                                 const std::string& filePath,
                                 const std::vector<std::string>& headers,
                                 std::function<void(long long)> progressCallback) {
    std::lock_guard<std::mutex> lock(mutex_);

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    FILE* file = fopen(filePath.c_str(), "wb");
    if (!file) { curl_easy_cleanup(curl); return false; }

    struct curl_slist* headerList = nullptr;
    for (const auto& h : headers) headerList = curl_slist_append(headerList, h.c_str());
    headerList = curl_slist_append(headerList, ("User-Agent: " + opts_.userAgent).c_str());
    headerList = curl_slist_append(headerList, "Referer: https://www.bilibili.com");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    if (progressCallback) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressCallback);
    }

    CURLcode res = curl_easy_perform(curl);

    fclose(file);
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}

std::string CurlClient::urlEncode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    std::string result(encoded ? encoded : "");
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

std::string CurlClient::resolveRedirect(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);

    CURL* curl = curl_easy_init();
    std::string location;
    if (!curl) return url;

    static auto headerCb = [](char* buffer, size_t size, size_t nitems, void* userdata) -> size_t {
        auto* loc = static_cast<std::string*>(userdata);
        std::string header(buffer, size * nitems);
        if (header.find("Location:") == 0) {
            std::string val = header.substr(10);
            val.erase(std::remove(val.begin(), val.end(), '\r'), val.end());
            val.erase(std::remove(val.begin(), val.end(), '\n'), val.end());
            *loc = val;
        }
        return size * nitems;
    };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, +headerCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &location);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return location.empty() ? url : location;
}

} // namespace narnat
