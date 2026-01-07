#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iomanip>

#define PORT 8081
#define BUFFER_SIZE 4096
#define MAX_CHUNK_SIZE (2 * 1024 * 1024) // 2MB 限制

// 打印十六进制用于调试
void hex_dump(const void* data, size_t size) {
    const unsigned char* bytes = (const unsigned char*)data;
    size_t i;
    for (i = 0; i < size; i++) {
        if (i % 16 == 0) {
            if (i > 0) std::cout << std::endl;
            std::cout << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

// 记录请求详情
void log_request(const std::string& request) {
    std::cout << "\n=== 收到请求 ===" << std::endl;
    size_t end = request.find("\r\n\r\n");
    if (end == std::string::npos) {
        end = request.length();
    }
    std::cout << request.substr(0, end) << std::endl;
    
    // 查找 Range 头部
    size_t range_start = request.find("Range: ");
    if (range_start != std::string::npos) {
        size_t range_end = request.find("\r\n", range_start);
        std::cout << "Range头部: " << request.substr(range_start, range_end - range_start) << std::endl;
    }
}

// 获取文件大小
long long get_file_size(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) return -1;
    return file.tellg();
}

bool parse_range_header(const std::string& request, long long& start, long long& end, long long file_size) {
    std::regex range_regex("Range:\\s*bytes=(\\d+)-(\\d*)", std::regex_constants::icase);
    std::smatch match;
    
    if (std::regex_search(request, match, range_regex) && match.size() >= 3) {
        try {
            start = std::stoll(match[1].str());
            std::string end_str = match[2].str();
            
            if (!end_str.empty()) {
                end = std::stoll(end_str);
            } else {
                end = file_size - 1;
            }
            
            // 验证范围
            if (start < 0 || start >= file_size) {
                std::cerr << "无效的起始位置: " << start << "，文件大小: " << file_size << std::endl;
                return false;
            }
            
            if (end >= file_size) {
                end = file_size - 1;
            }
            
            if (start > end) {
                std::cerr << "起始位置大于结束位置: " << start << " > " << end << std::endl;
                return false;
            }
            
            // 限制块大小
            long long requested_size = end - start + 1;
            if (requested_size > MAX_CHUNK_SIZE) {
                end = start + MAX_CHUNK_SIZE - 1;
                if (end >= file_size) {
                    end = file_size - 1;
                }
                std::cout << "请求大小 " << requested_size << " 超过限制，调整为 " << (end - start + 1) << std::endl;
            }
            
            std::cout << "解析Range: start=" << start << ", end=" << end 
                      << ", size=" << (end - start + 1) << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "解析Range失败: " << e.what() << std::endl;
            return false;
        }
    }
    return false;
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return "";
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content(size, '\0');
    if (file.read(&content[0], size)) {
        return content;
    }
    return "";
}

void send_response(int client_socket, const std::string& content, 
                   const std::string& content_type = "text/html", 
                   int status_code = 200,
                   const std::string& extra_headers = "") {
    std::stringstream response;
    
    switch(status_code) {
        case 200: response << "HTTP/1.1 200 OK\r\n"; break;
        case 206: response << "HTTP/1.1 206 Partial Content\r\n"; break;
        case 404: response << "HTTP/1.1 404 Not Found\r\n"; break;
        case 416: response << "HTTP/1.1 416 Range Not Satisfiable\r\n"; break;
        case 500: response << "HTTP/1.1 500 Internal Server Error\r\n"; break;
        default: response << "HTTP/1.1 200 OK\r\n";
    }
    
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Accept-Ranges: bytes\r\n";
    response << "Connection: close\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    
    if (!extra_headers.empty()) {
        response << extra_headers;
    }
    
    response << "\r\n" << content;
    
    std::string response_str = response.str();
    std::cout << "发送响应: " << status_code << "，长度: " << content.length() << std::endl;
    
    ssize_t sent = send(client_socket, response_str.c_str(), response_str.length(), 0);
    if (sent < 0) {
        std::cerr << "发送响应失败: " << strerror(errno) << std::endl;
    }
}

void send_audio_with_range(int client_socket, const std::string& filename, const std::string& request) {
    std::cout << "\n处理音频请求: " << filename << std::endl;
    
    // 获取文件大小
    long long file_size = get_file_size(filename);
    if (file_size < 0) {
        std::cerr << "文件不存在或无法访问: " << filename << std::endl;
        send_response(client_socket, "音频文件未找到", "text/plain", 404);
        return;
    }
    
    std::cout << "文件大小: " << file_size << " 字节 (" 
              << (file_size / 1024.0 / 1024.0) << " MB)" << std::endl;
    
    // 尝试打开文件
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件进行读取" << std::endl;
        send_response(client_socket, "无法读取音频文件", "text/plain", 500);
        return;
    }
    
    // 解析Range头部
    long long start = 0;
    long long end = file_size - 1;
    bool has_range = parse_range_header(request, start, end, file_size);
    
    // 验证并调整范围
    if (has_range) {
        if (start >= file_size || start < 0) {
            std::stringstream content_range;
            content_range << "bytes */" << file_size;
            send_response(client_socket, "范围无效", "text/plain", 416,
                         "Content-Range: " + content_range.str() + "\r\n");
            return;
        }
    } else {
        // 没有Range，发送文件开头部分
        end = std::min((long long)MAX_CHUNK_SIZE, file_size) - 1;
    }
    
    long long chunk_size = end - start + 1;
    std::cout << "准备发送: start=" << start << ", end=" << end 
              << ", size=" << chunk_size << std::endl;
    
    // 分配缓冲区
    std::vector<char> buffer(chunk_size);
    
    // 定位并读取
    file.seekg(start, std::ios::beg);
    if (!file.read(buffer.data(), chunk_size)) {
        std::cerr << "读取文件失败" << std::endl;
        send_response(client_socket, "读取文件失败", "text/plain", 500);
        return;
    }
    
    // 构建响应头
    std::stringstream response;
    if (has_range) {
        response << "HTTP/1.1 206 Partial Content\r\n";
        response << "Content-Range: bytes " << start << "-" << end 
                 << "/" << file_size << "\r\n";
    } else {
        response << "HTTP/1.1 200 OK\r\n";
    }
    
    response << "Content-Type: audio/mp4\r\n";
    response << "Content-Length: " << chunk_size << "\r\n";
    response << "Accept-Ranges: bytes\r\n";
    response << "Connection: close\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Cache-Control: no-cache, no-store, must-revalidate\r\n";
    response << "\r\n";
    
    std::string header = response.str();
    std::cout << "发送响应头(" << header.length() << "字节):\n" << header << std::endl;
    
    // 发送头部
    ssize_t sent = send(client_socket, header.c_str(), header.length(), 0);
    if (sent != (ssize_t)header.length()) {
        std::cerr << "发送头部失败，期望 " << header.length() 
                  << "，实际 " << sent << std::endl;
        return;
    }
    
    // 发送数据
    std::cout << "开始发送音频数据..." << std::endl;
    sent = send(client_socket, buffer.data(), buffer.size(), 0);
    if (sent != (ssize_t)buffer.size()) {
        std::cerr << "发送数据不完整，期望 " << buffer.size() 
                  << "，实际 " << sent << std::endl;
    } else {
        std::cout << "音频数据发送完成: " << sent << " 字节" << std::endl;
    }
    
    // 显示文件前几个字节用于调试
    if (start == 0 && chunk_size >= 8) {
        std::cout << "文件开头字节(hex): ";
        for (int i = 0; i < 8 && i < chunk_size; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                     << (int)(unsigned char)buffer[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
    file.close();
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        std::cout << "客户端断开连接或读取失败" << std::endl;
        close(client_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    std::string request(buffer);
    
    log_request(request);
    
    // 检查请求类型
    if (request.find("GET / ") == 0 || request.find("GET /index.html") == 0) {
        std::cout << "\n处理首页请求" << std::endl;
        std::string html = read_file("index.html");
        if (html.empty()) {
            std::cerr << "无法读取index.html，使用默认HTML" << std::endl;
            html = "<!DOCTYPE html><html><head><title>Audio Player</title></head><body>"
                   "<h2>Audio Player</h2><audio controls><source src='/audio.m4a' type='audio/mp4'>"
                   "Your browser does not support the audio element.</audio></body></html>";
        }
        send_response(client_socket, html, "text/html");
    }
    else if (request.find("GET /audio.m4a") == 0) {
        send_audio_with_range(client_socket, "audio.m4a", request);
    }
    else if (request.find("GET /favicon.ico") == 0) {
        // 忽略favicon请求
        send_response(client_socket, "", "image/x-icon", 404);
    }
    else if (request.find("GET / ") == 0) {
        std::string redirect = "HTTP/1.1 302 Found\r\nLocation: /index.html\r\n\r\n";
        send(client_socket, redirect.c_str(), redirect.length(), 0);
    }
    else {
        std::cout << "未知请求: " << request.substr(0, request.find('\n')) << std::endl;
        send_response(client_socket, "404 Not Found", "text/plain", 404);
    }
    
    close(client_socket);
    std::cout << "连接关闭" << std::endl;
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // 检查文件是否存在
    std::cout << "检查文件..." << std::endl;
    if (get_file_size("audio.m4a") < 0) {
        std::cerr << "错误: audio.m4a 文件不存在!" << std::endl;
        std::cerr << "请将音频文件放置在当前目录并命名为 audio.m4a" << std::endl;
    }
    
    if (get_file_size("index.html") < 0) {
        std::cerr << "警告: index.html 文件不存在，将使用默认HTML" << std::endl;
    }
    
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "创建socket失败: " << strerror(errno) << std::endl;
        return 1;
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "设置socket选项失败: " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // 绑定端口
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "绑定端口失败: " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    
    // 监听
    if (listen(server_fd, 10) < 0) {
        std::cerr << "监听失败: " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    
    std::cout << "\n==========================================" << std::endl;
    std::cout << "HTTP Audio Streaming Server" << std::endl;
    std::cout << "端口: " << PORT << std::endl;
    std::cout << "当前目录: " << std::endl;
    system("pwd && ls -la");
    std::cout << "==========================================" << std::endl;
    std::cout << "服务器已启动，等待连接..." << std::endl;
    
    // 主循环
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
            continue;
        }
        
        std::cout << "\n>>> 新的客户端连接" << std::endl;
        handle_client(client_socket);
    }
    
    close(server_fd);
    return 0;
}