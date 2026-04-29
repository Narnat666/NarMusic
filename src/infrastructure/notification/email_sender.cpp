#include "email_sender.h"
#include "core/logger.h"
#include <curl/curl.h>
#include <ctime>
#include <sstream>

namespace narnat {

static std::string base64Encode(const std::string& input) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(4 * (input.size() / 3 + 1));

    int val = 0, valb = -6;
    for (std::string::size_type i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(table[((val << 5) & 0x3F) | ((valb + 6) >> 1)]);
    while (out.size() % 4)
        out.push_back('=');

    return out;
}

static std::string formatDateHeader() {
    char buf[128];
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::gmtime(&now);
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S +0000", tm);
    return std::string(buf);
}

bool EmailSender::sendTo(const EmailConfig& config, const EmailAccount& account,
                         const std::string& subject, const std::string& body) {
    if (account.sender.empty() || account.password.empty() || account.receiver.empty()) {
        LOG_W("EmailSender", "邮箱账户配置不完整，跳过发送");
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_E("EmailSender", "curl_easy_init 失败");
        return false;
    }

    std::string dateStr = formatDateHeader();

    std::stringstream payload;
    payload << "Date: " << dateStr << "\r\n";
    payload << "To: " << account.receiver << "\r\n";
    payload << "From: NarMusic <" << account.sender << ">\r\n";
    payload << "Subject: =?UTF-8?B?" << base64Encode(subject) << "?=\r\n";
    payload << "MIME-Version: 1.0\r\n";
    payload << "Content-Type: text/plain; charset=UTF-8\r\n";
    payload << "Content-Transfer-Encoding: base64\r\n";
    payload << "\r\n";
    payload << base64Encode(body) << "\r\n";

    std::string payloadStr = payload.str();

    struct UploadCtx {
        const std::string* data;
        size_t pos = 0;
    };
    UploadCtx uploadCtx{&payloadStr, 0};

    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        auto* uc = static_cast<UploadCtx*>(userdata);
        size_t avail = uc->data->size() - uc->pos;
        if (avail == 0) return 0;
        size_t n = std::min(size * nmemb, avail);
        memcpy(ptr, uc->data->data() + uc->pos, n);
        uc->pos += n;
        return n;
    });
    curl_easy_setopt(curl, CURLOPT_READDATA, &uploadCtx);

    std::string smtpUrl = "smtp://" + config.smtp_host + ":" + std::to_string(config.smtp_port);
    curl_easy_setopt(curl, CURLOPT_URL, smtpUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));
    curl_easy_setopt(curl, CURLOPT_USERNAME, account.sender.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, account.password.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, account.sender.c_str());

    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, account.receiver.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_E("EmailSender", "邮件发送失败 (" + account.sender + " -> " + account.receiver + "): " + std::string(curl_easy_strerror(res)));
        return false;
    }

    LOG_I("EmailSender", "邮件已发送: " + account.sender + " -> " + account.receiver);
    return true;
}

bool EmailSender::sendAll(const EmailConfig& config,
                          const std::string& subject, const std::string& body) {
    if (config.accounts.empty()) {
        LOG_W("EmailSender", "无邮箱账户配置，跳过发送");
        return false;
    }

    bool anySuccess = false;
    for (const auto& account : config.accounts) {
        if (sendTo(config, account, subject, body)) {
            anySuccess = true;
        }
    }
    return anySuccess;
}

} // namespace narnat
