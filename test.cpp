#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <sstream>

#define PORT 8081
#define BUFFER_SIZE 4096

std::string read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return "";
    
    std::string content;
    file.seekg(0, std::ios::end);
    content.reserve(file.tellg());
    file.seekg(0, std::ios::beg);
    content.assign((std::istreambuf_iterator<char>(file)), 
                    std::istreambuf_iterator<char>());
    return content;
}

void send_response(int client_socket, const std::string& content, 
                   const std::string& content_type = "text/html") {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "\r\n" << content;
    
    std::string response_str = response.str();
    send(client_socket, response_str.c_str(), response_str.length(), 0);
}

void send_binary_file(int client_socket, const std::string& filename, 
                      const std::string& content_type) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        send_response(client_socket, "File not found", "text/plain");
        return;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: " << content_type << "\r\n";
        response << "Content-Length: " << size << "\r\n";
        response << "Accept-Ranges: bytes\r\n";
        response << "Connection: close\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        
        std::string header = response.str();
        send(client_socket, header.c_str(), header.length(), 0);
        send(client_socket, buffer.data(), buffer.size(), 0);
    }
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    std::string request(buffer);
    
    if (request.find("GET / ") == 0 || request.find("GET /index.html") != std::string::npos) {
        std::string html = read_file("index.html");
        if (html.empty()) {
            html = "<h1>Audio Player</h1><p>Place index.html in this directory</p>";
        }
        send_response(client_socket, html, "text/html");
    }
    else if (request.find("GET /audio.m4a") != std::string::npos) {
        send_binary_file(client_socket, "audio.m4a", "audio/mp4");
    }
    else if (request.find("GET / ") == 0) {
        // 重定向到index.html
        std::string redirect = "HTTP/1.1 302 Found\r\nLocation: /index.html\r\n\r\n";
        send(client_socket, redirect.c_str(), redirect.length(), 0);
    }
    else {
        send_response(client_socket, "404 Not Found", "text/plain");
    }
    
    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket failed" << std::endl;
        return 1;
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "Setsockopt failed" << std::endl;
        close(server_fd);
        return 1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // 绑定端口
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(server_fd);
        return 1;
    }
    
    // 监听
    if (listen(server_fd, 1) < 0) {  // 只允许一个连接排队
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return 1;
    }
    
    std::cout << "Simple Audio Server started at http://0.0.0.0:" << PORT << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // 单线程循环
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        handle_client(client_socket);
    }
    
    close(server_fd);
    return 0;
}

// /home/saisi/rk3568_linux_sdk/buildroot/output/my/host/bin/aarch64-buildroot-linux-gnu-g++ -O2 -std=c++11 -static -pthread -Wall test.cpp -o test