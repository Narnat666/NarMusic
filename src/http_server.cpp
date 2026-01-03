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
#include "task_manger.h"
#include "nlohmann/json.hpp"
#include "curl/curl.h"
#include <ifaddrs.h>
#include <net/if.h>

using json = nlohmann::json;
extern bool debug;

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

std::string getTaskIdByJson(std::string& query) {
    size_t pos = query.find("task_id=");
    std::string task_id = "";
    if (pos != std::string::npos) {
        task_id = query.substr(pos + 8);
        // 移除可能的后缀
        size_t end = task_id.find('&');
        if (end != std::string::npos) {
            task_id = task_id.substr(0, end);
        }
    }
    return task_id;
}

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) { // 短链接回调函数
    std::string* location = static_cast<std::string*>(userdata);
    std::string header(buffer, size * nitems);
    
    // 查找Location头部
    if (header.find("Location:") == 0) {
        std::string loc = header.substr(10);
        // 去除末尾的换行符
        loc.erase(std::remove(loc.begin(), loc.end(), '\r'), loc.end());
        loc.erase(std::remove(loc.begin(), loc.end(), '\n'), loc.end());
        *location = loc;
    }
    
    return size * nitems;
}

std::string resolveShortUrl(const std::string& shortUrl) {  // 解析短链接函数
    CURL* curl = curl_easy_init();
    std::string location;
    
    if (curl) {
        // 设置CURL选项
        curl_easy_setopt(curl, CURLOPT_URL, shortUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &location);
        
        // 禁用SSL证书验证（开发环境使用）
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        // 设置超时和用户代理
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
        
        // 执行请求
        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "CURL错误: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_easy_cleanup(curl);
        
        // 如果找到了重定向位置，返回它
        if (!location.empty()) {
            if (debug) std::cout << "解析到重定向链接: " << location << std::endl;
            return location;
        }
    }
    
    // 如果解析失败，返回原链接
    return shortUrl;
}

std::string urlEncodeUtf8(const std::string& utf8Str) { // 字符串转码utf8
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (size_t i = 0; i < utf8Str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(utf8Str[i]);
        
        // 判断UTF-8字符字节数
        if (c <= 0x7F) {  // ASCII字符
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << static_cast<char>(c);
            } else {
                escaped << '%' << std::setw(2) << std::uppercase << static_cast<int>(c);
            }
        } 
        else if ((c & 0xE0) == 0xC0) {  // 2字节UTF-8
            // 编码两个字节
            escaped << '%' << std::setw(2) << std::uppercase << static_cast<int>(c);
            if (i + 1 < utf8Str.size()) {
                escaped << '%' << std::setw(2) << std::uppercase 
                        << static_cast<int>(static_cast<unsigned char>(utf8Str[++i]));
            }
        }
        else if ((c & 0xF0) == 0xE0) {  // 3字节UTF-8
            // 编码三个字节
            for (int j = 0; j < 3 && i < utf8Str.size(); ++j, ++i) {
                escaped << '%' << std::setw(2) << std::uppercase 
                        << static_cast<int>(static_cast<unsigned char>(utf8Str[i]));
            }
            --i;  // 循环会多递增一次
        }
        else if ((c & 0xF8) == 0xF0) {  // 4字节UTF-8
            // 编码四个字节
            for (int j = 0; j < 4 && i < utf8Str.size(); ++j, ++i) {
                escaped << '%' << std::setw(2) << std::uppercase 
                        << static_cast<int>(static_cast<unsigned char>(utf8Str[i]));
            }
            --i;  // 循环会多递增一次
        }
    }
    
    return escaped.str();
}


std::string linkCut(std::string& link) {
    size_t start = link.find("http"); // 找到链接
    if (start == std::string::npos) return "";
    size_t end = link.find(" ", start);
    if (end == std::string::npos) end = link.length();

    return link.substr(start, end - start);
}


void HttpServer::handleRequest(int clientSocket) {
    HttpRequest request(clientSocket);
    if (!request.parse()) {
        sendResponse(clientSocket, "400 Bad Request", "{\"error\": \"Invalid request\"}");
        close(clientSocket);
        return;
    }


    // GET操作
    if (request.getMethod() == "GET") {

        // 获取页面信息操作
        std::string path = request.getPath();
        // 如果请求的是根路径，返回 index.html
        if (path == "/") {
            path = "/index.html";
        }
        // 拼接文件路径（确保路径安全）
        std::string filepath = "web" + path;
        // 安全检查：防止目录遍历攻击
        if (path.find("..") != std::string::npos) {
            sendResponse(clientSocket, "403 Forbidden", "{\"error\": \"Invalid path\"}");
            close(clientSocket);
            return;
        }
        // 检查文件是否存在
        if (fileExists(filepath)) {
            // 读取文件内容
            std::string content = readFile(filepath);
            if (!content.empty()) {
                // 根据文件扩展名设置正确的Content-Type
                std::string contentType = "text/plain";
                if (filepath.find(".html") != std::string::npos) {
                    contentType = "text/html; charset=utf-8";
                } else if (filepath.find(".css") != std::string::npos) {
                    contentType = "text/css; charset=utf-8";
                } else if (filepath.find(".js") != std::string::npos) {
                    contentType = "application/javascript; charset=utf-8";
                } else if (filepath.find(".json") != std::string::npos) {
                    contentType = "application/json";
                } else if (filepath.find(".png") != std::string::npos) {
                    contentType = "image/png";
                } else if (filepath.find(".jpg") != std::string::npos || filepath.find(".jpeg") != std::string::npos) {
                    contentType = "image/jpeg";
                }
                
                // 发送HTTP响应
                std::stringstream response;
                response << "HTTP/1.1 200 OK\r\n";
                response << "Content-Type: " << contentType << "\r\n";
                response << "Content-Length: " << content.length() << "\r\n";
                response << "Cache-Control: max-age=3600\r\n"; // 静态文件缓存1小时
                response << "\r\n";
                response << content;
                
                send(clientSocket, response.str().c_str(), response.str().length(), 0);
                close(clientSocket);
                
                if (debug) std::cout << "发送静态文件: " << filepath << " (" << content.length() << " 字节)" << std::endl;
                return;
            }
        }        

        // 处理status 请求
        if (request.getPath() == "/api/download/status") {
            std::string query = request.getQueryString();
            std::string task_id;
            
            task_id = getTaskIdByJson(query);
            
            if (task_id.empty()) {
                std::cerr << "找不到任务id：" << task_id << std::endl;
                sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"missing_task_id\"}");
                close(clientSocket);
                return;
            }
            
            // 获取任务状态
            std::string status = TaskManager::instance().getTaskStatus(task_id);
            sendResponse(clientSocket, "200 OK", status);
            close(clientSocket);
            return;
        }

        // 文件下载请求
        if (request.getPath() == "/api/download/file") {
            std::string query = request.getQueryString();
            std::string task_id;
            TaskManager& nt = TaskManager::instance();
            task_id = getTaskIdByJson(query);
            
            if (task_id.empty()) {
                std::cerr << "找不到任务id：" << task_id << std::endl;
                sendResponse(clientSocket, "400 Missing Task Id", "{\"error\":\"missing_task_id\"}");
                close(clientSocket);
                return;
            }

            // 查看文件是否还在
            auto it = nt.tasks_.find(task_id);
            { 
                // 开始上锁防止下载时文件被删掉
                std::lock_guard<std::mutex> lock(nt.mutex_);
                if (it != nt.tasks_.end()) { // 找到文件准备发送

                    // 查看文件是否下载成功
                    if (!it->second.is_finished) {
                        sendResponse(clientSocket, "404 Need Wait Task Finish", "{\"error\":\"please wait task finish\"}");
                        close(clientSocket);
                        return;
                    }

                    // 查看文件是否存在
                    MusicAnaly analy;
                    std::string file_name_path = it->second.file_path_name;
                    if (file_name_path.empty() || !analy.fileExists(file_name_path)) {
                        sendResponse(clientSocket, "404 File Deleted", "{\"error\":\"file missing\"}");
                        close(clientSocket);
                        return;
                    }

                    // 发送文件
                    std::cout << "文件发送开始：" << file_name_path << std::endl;

                    // 读取文件内容
                    std::ifstream file(file_name_path, std::ios::binary);
                    if (!file.is_open()) {
                        sendResponse(clientSocket, "500 Internal Server Error", "{\"error\":\"cannot_open_file\"}");
                        close(clientSocket);
                        return;
                    }
                
                    // 获取文件大小
                    file.seekg(0, std::ios::end);
                    size_t fileSize = file.tellg();
                    file.seekg(0, std::ios::beg);
                
                    // 读取文件内容到缓冲区
                    std::vector<char> buffer(fileSize);
                    file.read(buffer.data(), fileSize);
                    file.close();
                
                    // 获取文件名
                    std::string filename = it->second.file_send_name;

                    // 发送文件
                    std::stringstream response;
                    response << "HTTP/1.1 200 OK\r\n";
                    response << "Content-Type: application/octet-stream\r\n";
                    response << "Content-Disposition: attachment; filename=\"" << urlEncodeUtf8(filename) << "\"\r\n";
                    response << "Content-Length: " << fileSize << "\r\n";
                    response << "\r\n";
                
                    // 先发送头部
                    std::string headers = response.str();
                    send(clientSocket, headers.c_str(), headers.length(), 0);
                
                    // 然后发送文件内容
                    send(clientSocket, buffer.data(), buffer.size(), 0);
                
                    std::cout << "文件发送成功: " << filename << " (" << fileSize << " 字节)" << std::endl;
                
                    close(clientSocket);
                    return;

        
                }
            }
    
        }    
    
    }

    if (request.getMethod() == "POST" && request.getPath() == "/api/message") {
            std::string body = request.getBody();
            
            // 使用nlohmann/json解析JSON中的content字段
            std::string content = "";
            std::string rawContent = "";
            try {
                json j = json::parse(body);
                if (j.contains("content") && j["content"].is_string()) {
                    rawContent = j["content"].get<std::string>();
                    // 裁剪链接
                    rawContent = linkCut(rawContent);
                    // 转换短链接为标准链接
                    content = resolveShortUrl(rawContent);
                    
                   if (debug) std::cout << "原始URL: " << rawContent << std::endl;
                   if (debug) std::cout << "转换后URL: " << content << std::endl;
                } else {
                    sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"Missing or invalid content field\"}");
                    close(clientSocket);
                    return;
                }
            } catch (const json::parse_error& e) {
                sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"Invalid JSON format\"}");
                close(clientSocket);
                return;
            }

            // 提取文件名
            std::string file_name = ""; // 默认文件名
        
            try {
                json j = json::parse(body);      
                
                // 提取filename字段
                if (j.contains("filename") && j["filename"].is_string()) {
                    file_name = j["filename"].get<std::string>();
                    if (debug) std::cout << "使用自定义文件名: " << file_name << std::endl;
                } else {
                    if (debug) std::cout << "自定义文件名为空，将使用系统文件名" << std::endl;
                }
                
            } catch (const json::parse_error& e) {
                sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"Invalid JSON format\"}");
                close(clientSocket);
                return;
            }
            
            // 创建下载任务
            std::string task_id = TaskManager::instance().createTask(content, file_name);
            
            // 使用nlohmann/json创建JSON响应
            json responseJson;
            responseJson["task_id"] = task_id;
            responseJson["message"] = "download_started";
            responseJson["url"] = content;
            
            if(debug) std::cout << "创建任务id：" << task_id << " 链接：" << content << std::endl;
            
            sendResponse(clientSocket, "200 OK", responseJson.dump());
            close(clientSocket);

        } else {
            sendResponse(clientSocket, "404 Not Found", "{\"error\": \"Not found\"}");
        }
        
        close(clientSocket);
}

HttpServer::HttpServer(int port) : port_(port), serverSocket_(-1) {}

void HttpServer::start() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "创建套接字失败！" << std::endl;
        return;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(serverSocket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "套接字绑定失败！" << std::endl;
        close(serverSocket_);
        return;
    }

    if (listen(serverSocket_, 3) < 0) {
        std::cerr << "监听失败！" << std::endl;
        close(serverSocket_);
        return;
    }

    std::cout << "服务器启动到端口：" << port_ << std::endl;

    // 查看具体绑定到哪个ip
    std::cout << "服务器监听地址：" << std::endl;
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == 0) {
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            
            // 只显示IPv4地址，且排除回环接口
            if (ifa->ifa_addr->sa_family == AF_INET && 
                !(ifa->ifa_flags & IFF_LOOPBACK)) {
                char ip[INET_ADDRSTRLEN];
                void* addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                inet_ntop(AF_INET, addr, ip, sizeof(ip));
                std::cout << "  - " << ifa->ifa_name << ": http://" << ip << ":" << port_ << std::endl;
            }
        }
        freeifaddrs(ifaddr);
    }

    // 定时清理map表
    std::thread([&]() {  // 只捕获不拷贝
        while (true) { 
            TaskManager::instance().cleanupOldTasks(60);
            std::this_thread::sleep_for(std::chrono::seconds(60)); // 睡眠
        }
    }).detach();
    
    while (true) {
        sockaddr_in clientAddress;
        socklen_t clientLength = sizeof(clientAddress);
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddress, &clientLength);
        
        if (clientSocket < 0) {
            std::cerr << "接收错误！" << std::endl;
            continue;
        }
        
        handleRequest(clientSocket);
    }
}