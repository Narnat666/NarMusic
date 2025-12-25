#include "http_server.h"
#include "http_request.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <sstream>
#include <fstream>
#include <sys/stat.h>

void sendResponse(int socket, const std::string& status, const std::string& body) {
    std::stringstream response;
    response << "HTTP/1.1 " << status << "\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "\r\n";
    response << body;
    
    send(socket, response.str().c_str(), response.str().length(), 0);
}

// 辅助函数：读取整个文件
std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

// 辅助函数：检查文件是否存在
bool fileExists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}


void HttpServer::handleRequest(int clientSocket) {
    HttpRequest request(clientSocket);
    if (!request.parse()) {
        sendResponse(clientSocket, "400 Bad Request", "{\"error\": \"Invalid request\"}");
        close(clientSocket);
        return;
    }


    // 处理静态文件请求（GET）
    if (request.getMethod() == "GET") {
        std::string filepath = "web/index.html";
        // 如果请求的是根路径 "/"，返回 index.html
        if (request.getPath() == "/") {
            if (fileExists(filepath)) {
                std::string content = readFile(filepath);
                std::stringstream response;
                response << "HTTP/1.1 200 OK\r\n";
                response << "Content-Type: text/html; charset=utf-8\r\n";
                response << "Content-Length: " << content.length() << "\r\n";
                response << "\r\n";
                response << content;
                send(clientSocket, response.str().c_str(), response.str().length(), 0);
                close(clientSocket);
                return;
            } else {
                sendResponse(clientSocket, "500 Internal Error", "Missing web/index.html");
                close(clientSocket);
                return;
            }
        }
        // 可选：支持其他静态资源（如 /style.css 等）
        // 这里简化，只支持根路径
    }
    if (request.getMethod() == "POST" && request.getPath() == "/api/message") {
        std::string body = request.getBody();
        
        // 手动解析JSON中的content字段
        std::string content = "";
        size_t pos = body.find("\"content\":");
        if (pos != std::string::npos) {
            pos += 10; // 跳过"content": 
            if (body[pos] == '"') {
                pos++;
                size_t end = body.find('"', pos);
                if (end != std::string::npos) {
                    content = body.substr(pos, end - pos);
                }
            }
        }

        // 构造响应JSON
        std::stringstream response;
        response << "{";
        response << "\"received_content\": \"" << content << "\",";
        response << "\"server_time\": \"" << std::to_string(time(nullptr)) << "\",";
        response << "\"server_response\": \"Success\"";
        response << "}";
        
        sendResponse(clientSocket, "200 OK", response.str());
    } else {
        sendResponse(clientSocket, "404 Not Found", "{\"error\": \"Not found\"}");
    }
    
    close(clientSocket);
}

HttpServer::HttpServer(int port) : port_(port), serverSocket_(-1) {}

void HttpServer::start() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(serverSocket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(serverSocket_);
        return;
    }

    if (listen(serverSocket_, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(serverSocket_);
        return;
    }

    std::cout << "Server started on port " << port_ << std::endl;
    
    while (true) {
        sockaddr_in clientAddress;
        socklen_t clientLength = sizeof(clientAddress);
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddress, &clientLength);
        
        if (clientSocket < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        handleRequest(clientSocket);
    }
}