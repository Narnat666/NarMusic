#ifndef NARNAT_EPOLL_SERVER_H
#define NARNAT_EPOLL_SERVER_H

#include <string>
#include <atomic>
#include <memory>
#include <chrono>
#include <unordered_map>
#include "core/http/router.h"
#include "config/config.h"
#include "BS_thread_pool.hpp"
#include <mutex>

namespace narnat {

class EpollServer {
public:
    EpollServer(const ServerConfig& config, Router& router);
    ~EpollServer();

    void start();
    void stop();

private:
    // 创建监听socket
    bool createListenSocket();

    // 设置socket为非阻塞
    bool setNonBlocking(int fd);

    // 事件循环
    void eventLoop();

    // 处理新连接
    void handleAccept();

    // 处理读事件
    void handleRead(int fd);

    // 处理写事件
    void handleWrite(int fd);

    // 连接信息
    struct Connection {
        int fd = -1;
        std::string readBuffer;
        std::string writeBuffer;
        size_t written = 0;
        std::chrono::steady_clock::time_point lastActive;
        bool requestParsed = false;
    };

    // 清理超时连接
    void cleanupTimeouts();

    // 发送完整响应
    void sendResponse(int fd, const std::string& response);

    ServerConfig config_;
    Router& router_;
    BS::thread_pool<BS::tp::priority> pool_;

    int listenFd_ = -1;
    int epollFd_ = -1;
    int timerFd_ = -1;  // 定时器fd用于超时清理
    std::atomic<bool> running_{false};

    std::unordered_map<int, Connection> connections_;
    std::mutex connMutex_;

    static constexpr int MAX_EVENTS = 1024;
    static constexpr int READ_BUFFER_SIZE = 8192;
};

} // namespace narnat

#endif
