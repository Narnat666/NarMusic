#include "email_sender.h"
#include "core/logger.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <sstream>

namespace narnat {

static std::string base64Encode(const std::string& input) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(4 * (input.size() / 3 + 1));

    int val = 0, valb = -6;
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(table[(val << (-valb)) & 0x3F]);
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

struct SmtpConnection {
    int sock = -1;
    SSL* ssl = nullptr;
    SSL_CTX* ctx = nullptr;
    std::string lastError;

    bool connect(const std::string& host, int port) {
        struct hostent* he = gethostbyname(host.c_str());
        if (!he) {
            lastError = "DNS解析失败: " + host;
            return false;
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            lastError = "socket 创建失败";
            return false;
        }

        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], static_cast<size_t>(he->h_length));

        if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            lastError = "连接 " + host + ":" + std::to_string(port) + " 失败";
            close();
            return false;
        }

        return true;
    }

    bool startTls(const std::string& host) {
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            lastError = "SSL_CTX_new 失败";
            return false;
        }

        ssl = SSL_new(ctx);
        if (!ssl) {
            lastError = "SSL_new 失败";
            return false;
        }

        SSL_set_fd(ssl, sock);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        SSL_set_tlsext_host_name(ssl, host.c_str());
#pragma GCC diagnostic pop

        if (SSL_connect(ssl) != 1) {
            unsigned long err = ERR_get_error();
            char errBuf[256];
            ERR_error_string_n(err, errBuf, sizeof(errBuf));
            lastError = std::string("TLS握手失败: ") + errBuf;
            return false;
        }

        return true;
    }

    bool readResponse(std::string& out) {
        out.clear();
        char buf[1024];
        while (true) {
            int n;
            if (ssl) {
                n = SSL_read(ssl, buf, sizeof(buf) - 1);
            } else {
                n = static_cast<int>(::read(sock, buf, sizeof(buf) - 1));
            }
            if (n <= 0) break;
            buf[n] = '\0';
            out += buf;

            if (out.size() >= 4) {
                size_t lastCRLF = out.rfind("\r\n");
                if (lastCRLF != std::string::npos) {
                    size_t lineStart = out.rfind('\n', lastCRLF - 1);
                    if (lineStart == std::string::npos) lineStart = 0;
                    else lineStart++;
                    if (lastCRLF >= lineStart + 3 && out[lastCRLF - 3] != '-') {
                        break;
                    }
                }
            }
        }
        while (!out.empty() && (out.back() == '\r' || out.back() == '\n'))
            out.pop_back();
        return !out.empty();
    }

    bool sendCommand(const std::string& cmd) {
        std::string data = cmd + "\r\n";
        if (ssl) {
            return SSL_write(ssl, data.c_str(), static_cast<int>(data.size())) > 0;
        }
        return ::write(sock, data.c_str(), data.size()) == static_cast<ssize_t>(data.size());
    }

    int getResponseCode(const std::string& resp) {
        if (resp.size() >= 3) {
            try { return std::stoi(resp.substr(0, 3)); } catch (...) {}
        }
        return -1;
    }

    void close() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }
        if (ctx) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
        if (sock >= 0) {
            ::close(sock);
            sock = -1;
        }
    }
};

bool EmailSender::sendTo(const EmailConfig& config, const EmailAccount& account,
                         const std::string& subject, const std::string& body) {
    if (account.sender.empty() || account.password.empty() || account.receiver.empty()) {
        LOG_W("EmailSender", "邮箱账户配置不完整，跳过发送");
        return false;
    }

    SmtpConnection conn;
    std::string resp;

    if (!conn.connect(config.smtp_host, config.smtp_port)) {
        LOG_E("EmailSender", "连接失败: " + conn.lastError);
        conn.close();
        return false;
    }

    bool useImplicitSsl = (config.smtp_port == 465);

    if (useImplicitSsl) {
        if (!conn.startTls(config.smtp_host)) {
            LOG_E("EmailSender", "SSL握手失败: " + conn.lastError);
            conn.close();
            return false;
        }
        conn.readResponse(resp);
        if (conn.getResponseCode(resp) != 220) {
            LOG_E("EmailSender", "服务器未就绪: " + resp);
            conn.close();
            return false;
        }
    } else {
        conn.readResponse(resp);
        if (conn.getResponseCode(resp) != 220) {
            LOG_E("EmailSender", "服务器未就绪: " + resp);
            conn.close();
            return false;
        }

        conn.sendCommand("EHLO NarMusic");
        conn.readResponse(resp);

        conn.sendCommand("STARTTLS");
        conn.readResponse(resp);
        if (conn.getResponseCode(resp) != 220) {
            LOG_E("EmailSender", "STARTTLS 失败: " + resp);
            conn.close();
            return false;
        }

        if (!conn.startTls(config.smtp_host)) {
            LOG_E("EmailSender", "TLS握手失败: " + conn.lastError);
            conn.close();
            return false;
        }
    }

    conn.sendCommand("EHLO NarMusic");
    conn.readResponse(resp);
    if (conn.getResponseCode(resp) < 200 || conn.getResponseCode(resp) >= 400) {
        LOG_E("EmailSender", "EHLO 失败: " + resp);
        conn.close();
        return false;
    }

    std::string plainAuth = std::string("\0", 1) + account.sender + std::string("\0", 1) + account.password;
    conn.sendCommand("AUTH PLAIN " + base64Encode(plainAuth));
    conn.readResponse(resp);
    if (conn.getResponseCode(resp) != 235) {
        LOG_E("EmailSender", "认证失败: " + resp);
        conn.close();
        return false;
    }

    conn.sendCommand("MAIL FROM:<" + account.sender + ">");
    conn.readResponse(resp);
    if (conn.getResponseCode(resp) != 250) {
        LOG_E("EmailSender", "MAIL FROM 失败: " + resp);
        conn.close();
        return false;
    }

    conn.sendCommand("RCPT TO:<" + account.receiver + ">");
    conn.readResponse(resp);
    if (conn.getResponseCode(resp) != 250) {
        LOG_E("EmailSender", "RCPT TO 失败: " + resp);
        conn.close();
        return false;
    }

    conn.sendCommand("DATA");
    conn.readResponse(resp);
    if (conn.getResponseCode(resp) != 354) {
        LOG_E("EmailSender", "DATA 失败: " + resp);
        conn.close();
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
    payload << ".\r\n";

    std::string payloadStr = payload.str();
    if (conn.ssl) {
        SSL_write(conn.ssl, payloadStr.c_str(), static_cast<int>(payloadStr.size()));
    } else {
        if (::write(conn.sock, payloadStr.c_str(), payloadStr.size()) < 0) {
            conn.close();
            return false;
        }
    }

    conn.readResponse(resp);
    int code = conn.getResponseCode(resp);

    conn.sendCommand("QUIT");
    conn.readResponse(resp);
    conn.close();

    if (code != 250) {
        LOG_E("EmailSender", "邮件发送失败 (" + account.sender + " -> " + account.receiver + "): " + resp);
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
