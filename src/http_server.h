#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include "http_request.h"

class HttpServer {
public:
    HttpServer(int port, const std::string& root_dir);
    bool start();

private:
    void handleClient(int client_socket);
    std::string getMimeType(const std::string& path);
    std::string readFile(const std::string& path);
    void sendResponse(int client_socket, const std::string& response);
    void sendFile(int client_socket, const std::string& file_path);
    void handleGetRequest(int client_socket, const std::string& path);  // 处理get请求
    void handlePostRequest(int client_socket, const HttpRequest& request); // 处理set请求
    
    int port_;
    std::string root_dir_;
};

#endif