#include "http_server.h"
#include "http_request.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>

HttpServer::HttpServer(int port, const std::string& root_dir) 
    : port_(port), root_dir_(root_dir) {}

bool HttpServer::start() {
    // 创建套接字
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    // 设置套接字选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "Setsockopt failed" << std::endl;
        return false;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    // 绑定套接字
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return false;
    }

    // 监听
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return false;
    }

    std::cout << "Server started on port " << port_ << std::endl;
    std::cout << "Serving files from: " << root_dir_ << std::endl;

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }

        handleClient(client_socket);
        close(client_socket);
    }

    close(server_fd);
    return true;
}

void HttpServer::handleClient(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);

    HttpRequest request;
    request.parse(buffer);
    request.print();

    std::string path = request.getPath();
    if (path == "/") {
        path = "/index.html";
    }

    std::string full_path = root_dir_ + path;
    sendFile(client_socket, full_path);
}

std::string HttpServer::getMimeType(const std::string& path) {
    if (path.find(".html") != std::string::npos) {
        return "text/html";
    } else if (path.find(".css") != std::string::npos) {
        return "text/css";
    } else if (path.find(".txt") != std::string::npos) {
        return "text/plain";
    } else if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) {
        return "image/jpeg";
    } else if (path.find(".png") != std::string::npos) {
        return "image/png";
    }
    return "application/octet-stream";
}

std::string HttpServer::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void HttpServer::sendResponse(int client_socket, const std::string& response) {
    send(client_socket, response.c_str(), response.length(), 0);
}

void HttpServer::sendFile(int client_socket, const std::string& file_path) {
    std::string content = readFile(file_path);
    
    if (content.empty()) {
        std::string response = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        sendResponse(client_socket, response);
        return;
    }

    std::string mime_type = getMimeType(file_path);
    
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: " << mime_type << "\r\n"
             << "Content-Length: " << content.length() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << content;
    
    sendResponse(client_socket, response.str());
}