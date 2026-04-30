#include "epoll_server.h"
#include "core/logger.h"
#include "core/http/request.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <cstring>
#include <errno.h>
#include <fstream>

namespace narnat {

static constexpr size_t FILE_CHUNK_SIZE = 256 * 1024;

EpollServer::EpollServer(const ServerConfig& config, Router& router)
    : config_(config), router_(router), pool_(static_cast<size_t>(config.thread_pool_size)) {}

EpollServer::~EpollServer() {
    stop();
}

bool EpollServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool EpollServer::createListenSocket() {
    listenFd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listenFd_ < 0) {
        LOG_E("EpollServer", "创建监听socket失败");
        return false;
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(config_.port));

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_E("EpollServer", "绑定端口失败");
        return false;
    }

    if (listen(listenFd_, config_.backlog) < 0) {
        LOG_E("EpollServer", "监听失败");
        return false;
    }

    return true;
}

void EpollServer::start() {
    if (!createListenSocket()) return;

    epollFd_ = epoll_create1(0);
    if (epollFd_ < 0) {
        LOG_E("EpollServer", "创建epoll失败");
        return;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenFd_;
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd_, &ev) < 0) {
        LOG_E("EpollServer", "注册监听fd到epoll失败");
        return;
    }

    timerFd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerFd_ >= 0) {
        struct itimerspec its{};
        its.it_interval.tv_sec = 10;
        its.it_value.tv_sec = 10;
        timerfd_settime(timerFd_, 0, &its, nullptr);

        ev.events = EPOLLIN;
        ev.data.fd = timerFd_;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, timerFd_, &ev);
    }

    wakeupFd_ = eventfd(0, EFD_NONBLOCK);
    if (wakeupFd_ >= 0) {
        ev.events = EPOLLIN;
        ev.data.fd = wakeupFd_;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, wakeupFd_, &ev);
    }

    LOG_I("EpollServer", "服务器启动，端口: " + std::to_string(config_.port));
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == 0) {
        for (auto* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET ||
                (ifa->ifa_flags & IFF_LOOPBACK)) continue;
            char ip[INET_ADDRSTRLEN];
            auto* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
            LOG_I("EpollServer", std::string("  ") + ifa->ifa_name + ": http://" + ip + ":" + std::to_string(config_.port));
        }
        freeifaddrs(ifaddr);
    }

    running_ = true;
    eventLoop();
}

void EpollServer::stop() {
    running_ = false;

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        for (auto& [fd, conn] : connections_) {
            if (conn.fileFd >= 0) { close(conn.fileFd); conn.fileFd = -1; }
            close(fd);
        }
        connections_.clear();
    }

    if (wakeupFd_ >= 0) { close(wakeupFd_); wakeupFd_ = -1; }
    if (listenFd_ >= 0) { close(listenFd_); listenFd_ = -1; }
    if (epollFd_ >= 0) { close(epollFd_); epollFd_ = -1; }
    if (timerFd_ >= 0) { close(timerFd_); timerFd_ = -1; }
}

void EpollServer::eventLoop() {
    struct epoll_event events[MAX_EVENTS];

    while (running_) {
        int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, 1000);

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == listenFd_) {
                handleAccept();
            } else if (fd == timerFd_) {
                uint64_t expirations;
                ssize_t ret [[maybe_unused]] = read(timerFd_, &expirations, sizeof(expirations));
                cleanupTimeouts();
            } else if (fd == wakeupFd_) {
                uint64_t count;
                while (read(wakeupFd_, &count, sizeof(count)) == sizeof(count)) {}
                processPendingWrites();
            } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                std::lock_guard<std::mutex> lock(connMutex_);
                auto it = connections_.find(fd);
                if (it != connections_.end()) {
                    if (it->second.fileFd >= 0) close(it->second.fileFd);
                }
                close(fd);
                connections_.erase(fd);
            } else if (events[i].events & EPOLLIN) {
                handleRead(fd);
            } else if (events[i].events & EPOLLOUT) {
                handleWrite(fd);
            }
        }
    }
}

void EpollServer::handleAccept() {
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = accept4(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr),
                               &addrLen, SOCK_NONBLOCK);
        if (clientFd < 0) {
            if (errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) break;
            LOG_W("EpollServer", "accept错误: " + std::string(strerror(errno)));
            break;
        }

        int opt = 1;
        setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        int sndbuf = 256 * 1024;
        setsockopt(clientFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = clientFd;
        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, clientFd, &ev) < 0) {
            close(clientFd);
            continue;
        }

        std::lock_guard<std::mutex> lock(connMutex_);
        Connection& conn = connections_[clientFd];
        conn.fd = clientFd;
        conn.lastActive = std::chrono::steady_clock::now();
        conn.readBuffer.clear();
        conn.writeBuffer.clear();
        conn.written = 0;
        conn.requestParsed = false;
        conn.isFileStream = false;
        conn.fileFd = -1;
    }
}

void EpollServer::handleRead(int fd) {
    std::string requestData;
    char buffer[READ_BUFFER_SIZE];

    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            requestData.append(buffer, static_cast<size_t>(n));
        } else if (n == 0) {
            close(fd);
            std::lock_guard<std::mutex> lock(connMutex_);
            connections_.erase(fd);
            return;
        } else {
            if (errno != EAGAIN
#if EAGAIN != EWOULDBLOCK
                && errno != EWOULDBLOCK
#endif
            ) {
                close(fd);
                std::lock_guard<std::mutex> lock(connMutex_);
                connections_.erase(fd);
            }
            break;
        }
    }

    if (requestData.empty()) return;

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            it->second.lastActive = std::chrono::steady_clock::now();
            if (it->second.readBuffer.size() + requestData.size() > MAX_REQUEST_SIZE) {
                LOG_W("EpollServer", "请求超过大小限制，关闭连接 fd=" + std::to_string(fd));
                close(fd);
                connections_.erase(fd);
                return;
            }
            it->second.readBuffer += requestData;
        }
    }

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;
        if (!Request::isCompleteRequest(it->second.readBuffer)) return;
    }

    pool_.detach_task([this, fd]() {
        std::string taskData;
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            auto it = connections_.find(fd);
            if (it == connections_.end()) return;
            taskData = std::move(it->second.readBuffer);
        }

        Request req;
        if (!req.parse(taskData)) {
            auto resp = Response::error(400, "Bad Request", "bad_request", "Invalid request");
            enqueueResponse(fd, std::move(resp));
            return;
        }

        auto resp = router_.dispatch(req);
        enqueueResponse(fd, std::move(resp));
    });
}

void EpollServer::handleWrite(int fd) {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    Connection& conn = it->second;

    if (conn.isFileStream && conn.fileFd >= 0) {
        sendFileChunked(fd, conn);
        return;
    }

    while (conn.written < conn.writeBuffer.size()) {
        ssize_t n = send(fd, conn.writeBuffer.data() + conn.written,
                         conn.writeBuffer.size() - conn.written, MSG_NOSIGNAL);
        if (n > 0) {
            conn.written += static_cast<size_t>(n);
        } else if (n < 0) {
            if (errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return;
            }
            close(fd);
            connections_.erase(fd);
            return;
        } else {
            close(fd);
            connections_.erase(fd);
            return;
        }
    }

    conn.writeBuffer.clear();
    conn.written = 0;
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = fd;
    epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

void EpollServer::sendResponse(int fd, const std::string& response) {
    if (response.size() <= 65536) {
        size_t sent = 0;
        while (sent < response.size()) {
            ssize_t n = send(fd, response.data() + sent,
                             response.size() - sent, MSG_NOSIGNAL);
            if (n > 0) {
                sent += static_cast<size_t>(n);
            } else if (n < 0) {
                if (errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
                    || errno == EWOULDBLOCK
#endif
                ) {
                    std::lock_guard<std::mutex> lock(connMutex_);
                    auto it = connections_.find(fd);
                    if (it != connections_.end()) {
                        it->second.writeBuffer = response.substr(sent);
                        it->second.written = 0;
                        struct epoll_event ev{};
                        ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
                        ev.data.fd = fd;
                        epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
                    }
                    return;
                }
                return;
            } else {
                return;
            }
        }
    } else {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            it->second.writeBuffer = response;
            it->second.written = 0;
            struct epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
            ev.data.fd = fd;
            epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
        }
    }
}

void EpollServer::enqueueResponse(int fd, Response resp) {
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingWrites_.emplace_back(fd, std::move(resp));
    }
    uint64_t signal = 1;
    ssize_t ret [[maybe_unused]] = write(wakeupFd_, &signal, sizeof(signal));
}

void EpollServer::processPendingWrites() {
    std::vector<std::pair<int, Response>> writes;
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        writes.swap(pendingWrites_);
    }

    for (auto& [fd, resp] : writes) {
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            if (connections_.find(fd) == connections_.end()) continue;
        }

        if (resp.isFileStream()) {
            const auto& info = resp.fileStreamInfo();
            std::string headers = resp.serializeHeaders();
            sendResponse(fd, headers);

            int fileFd = open(info.filePath.c_str(), O_RDONLY);
            if (fileFd < 0) {
                LOG_E("EpollServer", "文件打开失败: " + info.filePath);
                std::lock_guard<std::mutex> lock(connMutex_);
                auto it = connections_.find(fd);
                if (it != connections_.end()) {
                    close(fd);
                    connections_.erase(fd);
                }
                continue;
            }

            if (info.rangeStart > 0) {
                off_t offset = info.rangeStart;
                lseek(fileFd, offset, SEEK_SET);
            }

            std::lock_guard<std::mutex> lock(connMutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                it->second.isFileStream = true;
                it->second.fileFd = fileFd;
                it->second.fileOffset = info.rangeStart;
                it->second.fileEnd = info.rangeEnd;
                it->second.cleanupPath = info.cleanupPath;
                it->second.writeBuffer.clear();
                it->second.written = 0;

                sendFileChunked(fd, it->second);
            } else {
                close(fileFd);
            }
        } else {
            sendResponse(fd, resp.serialize());
        }
    }
}

void EpollServer::sendFileChunked(int fd, Connection& conn) {
    if (conn.fileFd < 0) {
        conn.isFileStream = false;
        return;
    }

    while (conn.fileOffset <= conn.fileEnd) {
        long long remaining = conn.fileEnd - conn.fileOffset + 1;
        size_t toSend = remaining > static_cast<long long>(FILE_CHUNK_SIZE)
            ? FILE_CHUNK_SIZE : static_cast<size_t>(remaining);

        ssize_t n = sendfile(fd, conn.fileFd, nullptr, toSend);
        if (n > 0) {
            conn.fileOffset += n;
            if (conn.fileOffset > conn.fileEnd) break;
        } else if (n < 0) {
            if (errno == EAGAIN
#if EAGAIN != EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                struct epoll_event ev{};
                ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
                ev.data.fd = fd;
                epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
                return;
            }
            close(conn.fileFd);
            conn.fileFd = -1;
            if (!conn.cleanupPath.empty()) {
                unlink(conn.cleanupPath.c_str());
                conn.cleanupPath.clear();
            }
            close(fd);
            connections_.erase(fd);
            return;
        } else {
            break;
        }
    }

    close(conn.fileFd);
    conn.fileFd = -1;
    conn.isFileStream = false;

    if (!conn.cleanupPath.empty()) {
        unlink(conn.cleanupPath.c_str());
        conn.cleanupPath.clear();
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = fd;
    epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

void EpollServer::cleanupTimeouts() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(connMutex_);

    for (auto it = connections_.begin(); it != connections_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastActive).count();
        if (elapsed > config_.connection_timeout) {
            if (it->second.fileFd >= 0) close(it->second.fileFd);
            close(it->first);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace narnat
