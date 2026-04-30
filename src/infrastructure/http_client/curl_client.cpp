#include "curl_client.h"
#include "core/logger.h"
#include <curl/curl.h>
#include <fstream>

namespace narnat {

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

static int curlProgressCallback(void* clientp, curl_off_t, curl_off_t dlnow,
                                curl_off_t, curl_off_t) {
    auto* cb = static_cast<std::function<void(long long)>*>(clientp);
    if (cb && *cb && dlnow > 0) {
        (*cb)(dlnow);
    }
    return 0;
}

CurlClient::CurlClient() : opts_() {}

CurlClient::CurlClient(const Options& opts) : opts_(opts) {}

CurlClient::CurlHandle CurlClient::acquireHandle(const std::vector<std::string>& headers,
                                                   bool setUserAgent) {
    CurlHandle h;

    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        if (!handlePool_.empty()) {
            h.easy = handlePool_.front();
            handlePool_.pop_front();
        }
    }

    if (!h.easy) {
        h.easy = curl_easy_init();
    }

    if (!h.easy) return h;

    curl_easy_reset(h.easy);

    curl_slist* slist = nullptr;
    for (const auto& hdr : headers) {
        slist = curl_slist_append(slist, hdr.c_str());
    }
    if (setUserAgent && !opts_.userAgent.empty()) {
        slist = curl_slist_append(slist, ("User-Agent: " + opts_.userAgent).c_str());
    }

    if (slist) {
        curl_easy_setopt(h.easy, CURLOPT_HTTPHEADER, slist);
    }
    h.headers = slist;

    return h;
}

void CurlClient::releaseHandle(CurlHandle&& handle) {
    if (handle.headers) {
        curl_slist_free_all(static_cast<curl_slist*>(handle.headers));
    }

    if (handle.easy) {
        std::lock_guard<std::mutex> lock(poolMutex_);
        handlePool_.push_back(handle.easy);
    }
}

void CurlClient::applyCommonOptions(void* curl) {
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, opts_.requestTimeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, opts_.connectTimeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, opts_.followRedirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, opts_.verifySsl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, opts_.verifySsl ? 2L : 0L);
}

HttpResponse CurlClient::get(const std::string& url,
                              const std::vector<std::string>& headers) {
    auto h = acquireHandle(headers);
    HttpResponse resp;
    if (!h.easy) { resp.error = "curl init failed"; return resp; }

    curl_easy_setopt(h.easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h.easy, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(h.easy, CURLOPT_WRITEDATA, &resp.body);

    curl_easy_setopt(h.easy, CURLOPT_HEADERFUNCTION, +[](char* buffer, size_t size, size_t nitems, void* userdata) -> size_t {
        auto* hdrs = static_cast<std::vector<std::string>*>(userdata);
        std::string header(buffer, size * nitems);
        while (!header.empty() && (header.back() == '\r' || header.back() == '\n'))
            header.pop_back();
        if (!header.empty())
            hdrs->push_back(header);
        return size * nitems;
    });
    curl_easy_setopt(h.easy, CURLOPT_HEADERDATA, &resp.headers);

    applyCommonOptions(h.easy);

    CURLcode res = curl_easy_perform(h.easy);
    if (res == CURLE_OK) {
        curl_easy_getinfo(h.easy, CURLINFO_RESPONSE_CODE, &resp.statusCode);
        resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);
    } else {
        resp.error = curl_easy_strerror(res);
        LOG_W("CurlClient", "GET失败: " + resp.error + " url=" + url);
    }

    releaseHandle(std::move(h));
    return resp;
}

HttpResponse CurlClient::post(const std::string& url,
                               const std::string& data,
                               const std::vector<std::string>& headers) {
    auto h = acquireHandle(headers);
    HttpResponse resp;
    if (!h.easy) { resp.error = "curl init failed"; return resp; }

    curl_easy_setopt(h.easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h.easy, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(h.easy, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(h.easy, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(h.easy, CURLOPT_POST, 1L);
    applyCommonOptions(h.easy);

    CURLcode res = curl_easy_perform(h.easy);
    if (res == CURLE_OK) {
        curl_easy_getinfo(h.easy, CURLINFO_RESPONSE_CODE, &resp.statusCode);
        resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);
    } else {
        resp.error = curl_easy_strerror(res);
        LOG_W("CurlClient", "POST失败: " + resp.error);
    }

    releaseHandle(std::move(h));
    return resp;
}

HttpResponse CurlClient::download(const std::string& url,
                                   const std::vector<std::string>& headers) {
    auto h = acquireHandle(headers);
    HttpResponse resp;
    if (!h.easy) { resp.error = "curl init failed"; return resp; }

    curl_easy_setopt(h.easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h.easy, CURLOPT_WRITEFUNCTION, writeBinaryCallback);
    curl_easy_setopt(h.easy, CURLOPT_WRITEDATA, &resp.binaryBody);
    curl_easy_setopt(h.easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h.easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(h.easy, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(h.easy);
    if (res == CURLE_OK) {
        curl_easy_getinfo(h.easy, CURLINFO_RESPONSE_CODE, &resp.statusCode);
        resp.success = true;
    } else {
        resp.error = curl_easy_strerror(res);
    }

    releaseHandle(std::move(h));
    return resp;
}

bool CurlClient::downloadToFile(const std::string& url,
                                 const std::string& filePath,
                                 const std::vector<std::string>& headers,
                                 std::function<void(long long)> progressCallback) {
    auto h = acquireHandle(headers);
    if (!h.easy) return false;

    auto* slist = static_cast<curl_slist*>(h.headers);
    slist = curl_slist_append(slist, "Referer: https://www.bilibili.com");
    h.headers = slist;

    if (h.headers) {
        curl_easy_setopt(h.easy, CURLOPT_HTTPHEADER, static_cast<curl_slist*>(h.headers));
    }

    FILE* file = fopen(filePath.c_str(), "wb");
    if (!file) { releaseHandle(std::move(h)); return false; }

    curl_easy_setopt(h.easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h.easy, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(h.easy, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(h.easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h.easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(h.easy, CURLOPT_SSL_VERIFYHOST, 0L);

    if (progressCallback) {
        curl_easy_setopt(h.easy, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(h.easy, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
        curl_easy_setopt(h.easy, CURLOPT_XFERINFODATA, &progressCallback);
    }

    CURLcode res = curl_easy_perform(h.easy);

    fclose(file);
    releaseHandle(std::move(h));

    return res == CURLE_OK;
}

std::string CurlClient::urlEncode(const std::string& value) {
    void* curl = curl_easy_init();
    if (!curl) return value;
    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    std::string result(encoded ? encoded : "");
    curl_free(encoded);

    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        handlePool_.push_back(curl);
    }

    return result;
}

std::string CurlClient::resolveRedirect(const std::string& url) {
    auto h = acquireHandle({}, false);
    std::string location;
    if (!h.easy) return url;

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

    curl_easy_setopt(h.easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h.easy, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(h.easy, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(h.easy, CURLOPT_HEADERFUNCTION, +headerCb);
    curl_easy_setopt(h.easy, CURLOPT_HEADERDATA, &location);
    curl_easy_setopt(h.easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(h.easy, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(h.easy, CURLOPT_TIMEOUT, 10L);

    curl_easy_perform(h.easy);
    releaseHandle(std::move(h));

    return location.empty() ? url : location;
}

} // namespace narnat
