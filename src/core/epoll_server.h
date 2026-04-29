#ifndef NARNAT_EPOLL_SERVER_H
#define NARNAT_EPOLL_SERVER_H

#include <string>
#include <atomic>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <optional>
#include "core/http/router.h"
#include "core/http/response.h"
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
    bool createListenSocket();

    bool setNonBlocking(int fd);

    void eventLoop();

    void handleAccept();

    void handleRead(int fd);

    void handleWrite(int fd);

    struct Connection {
        int fd = -1;
        std::string readBuffer;
        std::string writeBuffer;
        size_t written = 0;
        std::chrono::steady_clock::time_point lastActive;
        bool requestParsed = false;
        FileStreamInfo fileStream;
        bool isFileStream = false;
        long long fileOffset = 0;
        long long fileEnd = 0;
        int fileFd = -1;
    };

    void cleanupTimeouts();

    void sendResponse(int fd, const std::string& response);

    void enqueueResponse(int fd, Response resp);

    void processPendingWrites();

    void sendFileChunked(int fd, Connection& conn);

    ServerConfig config_;
    Router& router_;
    BS::thread_pool<BS::tp::priority> pool_;

    int listenFd_ = -1;
    int epollFd_ = -1;
    int timerFd_ = -1;
    int wakeupFd_ = -1;
    std::atomic<bool> running_{false};

    std::unordered_map<int, Connection> connections_;
    std::mutex connMutex_;

    std::vector<std::pair<int, Response>> pendingWrites_;
    std::mutex pendingMutex_;

    static constexpr int MAX_EVENTS = 1024;
    static constexpr int READ_BUFFER_SIZE = 8192;
};

} // namespace narnat

#endif
