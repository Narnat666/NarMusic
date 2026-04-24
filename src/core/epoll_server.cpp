#include "epoll_server.h"
#include "core/logger.h"
#include "core/http/request.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <cstring>
#include <errno.h>

namespace narnat {

EpollServer::EpollServer(const ServerConfig& config, Router& router)
    : config_(config), router_(router), pool_(config.thread_pool_size) {}

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
    addr.sin_port = htons(config_.port);

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

    // 注册监听fd
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenFd_;
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd_, &ev) < 0) {
        LOG_E("EpollServer", "注册监听fd到epoll失败");
        return;
    }

    // 创建定时器fd用于超时清理
    timerFd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerFd_ >= 0) {
        struct itimerspec its{};
        its.it_interval.tv_sec = 10;  // 每10秒
        its.it_value.tv_sec = 10;
        timerfd_settime(timerFd_, 0, &its, nullptr);

        ev.events = EPOLLIN;
        ev.data.fd = timerFd_;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, timerFd_, &ev);
    }

    // 打印监听地址
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
                // 定时器触发：清理超时连接
                uint64_t expirations;
                read(timerFd_, &expirations, sizeof(expirations));
                cleanupTimeouts();
            } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                // 错误或挂起，关闭连接
                close(fd);
                std::lock_guard<std::mutex> lock(connMutex_);
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
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG_W("EpollServer", "accept错误: " + std::string(strerror(errno)));
            break;
        }

        // 设置TCP_NODELAY
        int opt = 1;
        setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        // 注册到epoll
        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = clientFd;
        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, clientFd, &ev) < 0) {
            close(clientFd);
            continue;
        }

        // 记录连接
        std::lock_guard<std::mutex> lock(connMutex_);
        Connection& conn = connections_[clientFd];
        conn.fd = clientFd;
        conn.lastActive = std::chrono::steady_clock::now();
        conn.readBuffer.clear();
        conn.writeBuffer.clear();
        conn.written = 0;
        conn.requestParsed = false;
    }
}

void EpollServer::handleRead(int fd) {
    std::string requestData;
    char buffer[READ_BUFFER_SIZE];

    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            requestData.append(buffer, n);
        } else if (n == 0) {
            // 连接关闭
            close(fd);
            std::lock_guard<std::mutex> lock(connMutex_);
            connections_.erase(fd);
            return;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close(fd);
                std::lock_guard<std::mutex> lock(connMutex_);
                connections_.erase(fd);
            }
            break;
        }
    }

    if (requestData.empty()) return;

    // 更新活跃时间
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            it->second.lastActive = std::chrono::steady_clock::now();
            it->second.readBuffer += requestData;
            requestData = it->second.readBuffer;
        }
    }

    // 提交到线程池处理
    pool_.detach_task([this, fd, requestData]() {
        Request req;
        if (!req.parse(requestData)) {
            auto resp = Response::error(400, "Bad Request", "bad_request", "Invalid request");
            sendResponse(fd, resp.serialize());
            return;
        }

        LOG_D("EpollServer", req.methodString() + " " + req.path());

        auto resp = router_.dispatch(req);
        sendResponse(fd, resp.serialize());
    });
}

void EpollServer::handleWrite(int fd) {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    Connection& conn = it->second;
    while (conn.written < conn.writeBuffer.size()) {
        ssize_t n = send(fd, conn.writeBuffer.data() + conn.written,
                         conn.writeBuffer.size() - conn.written, MSG_NOSIGNAL);
        if (n > 0) {
            conn.written += n;
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 等待下次EPOLLOUT
                return;
            }
            // 发送错误，关闭连接
            close(fd);
            connections_.erase(fd);
            return;
        } else {
            close(fd);
            connections_.erase(fd);
            return;
        }
    }

    // 发送完成，切换回EPOLLIN
    conn.writeBuffer.clear();
    conn.written = 0;
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = fd;
    epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

void EpollServer::sendResponse(int fd, const std::string& response) {
    // 对于小响应直接发送，大响应走epoll write
    if (response.size() <= 65536) {
        size_t sent = 0;
        while (sent < response.size()) {
            ssize_t n = send(fd, response.data() + sent,
                             response.size() - sent, MSG_NOSIGNAL);
            if (n > 0) {
                sent += n;
            } else if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 缓冲区满，走epoll write
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
                return; // 发送失败
            } else {
                return;
            }
        }
    } else {
        // 大响应走epoll write
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

void EpollServer::cleanupTimeouts() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(connMutex_);

    for (auto it = connections_.begin(); it != connections_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastActive).count();
        if (elapsed > config_.connection_timeout) {
            close(it->first);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace narnat
