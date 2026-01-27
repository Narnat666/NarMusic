#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <atomic>
#include "BS_thread_pool.hpp"

class HttpServer {
public:
    HttpServer(int port);
    ~HttpServer(); // 添加析构函数
    void start();
    void handleRequest(int clientSocket);
    
private:
    int port_;
    int serverSocket_;
    std::atomic<bool> running_{true};
    BS::thread_pool<BS::tp::priority> pool_{5};
    
};

#endif // HTTP_SERVER_H