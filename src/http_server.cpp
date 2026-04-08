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
#include <filesystem>
#include <map>
#include <vector>
#include <iomanip>
#include <algorithm>
#include "task_manger.h"
#include "nlohmann/json.hpp"
#include "curl/curl.h"
#include <ifaddrs.h>
#include <net/if.h>
#include "stream_send.h"

using json = nlohmann::json;
namespace fs = std::filesystem;
extern bool debug;
const long long MAX_CHUNK_SIZE = (288 * 1024);
TaskManager& nt = TaskManager::instance();

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

// 辅助函数：扫描音乐库目录
std::vector<std::map<std::string, std::string>> scanMusicLibrary(const std::string& directory) {
    std::vector<std::map<std::string, std::string>> musicList;
    
    try {
        // 检查目录是否存在
        if (!fs::exists(directory)) {
            std::cerr << "音乐库目录不存在: " << directory << std::endl;
            return musicList;
        }
        
        // 扫描目录中的音乐文件
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension();
                
                // 支持的音乐文件格式
                if (extension == ".m4a" || extension == ".mp3" || 
                    extension == ".flac" || extension == ".wav" ||
                    extension == ".aac" || extension == ".ogg") {
                    
                    std::map<std::string, std::string> musicItem;
                    
                    // 系统文件名
                    std::string system_filename = entry.path().filename();
                    musicItem["system_filename"] = system_filename;
                    
                    // 查找自定义文件名
                    std::string custom_filename = system_filename;
                    {
                        std::lock_guard<std::mutex> lock(nt.mutex_);
                        for (const auto& task : nt.tasks_) {
                            // 通过文件路径匹配查找对应的任务
                            std::string task_file_path = task.second.file_path_name;
                            if (task.second.is_finished && 
                                task_file_path.find(system_filename) != std::string::npos) {
                                custom_filename = task.second.file_send_name;
                                break;
                            }
                        }
                    }
                    musicItem["filename"] = custom_filename;
                    
                    // 文件大小（字节）
                    musicItem["file_size"] = std::to_string(entry.file_size());
                    
                    // 下载时间（使用文件修改时间）
                    auto ftime = entry.last_write_time();
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + 
                        std::chrono::system_clock::now());
                    time_t download_time = std::chrono::system_clock::to_time_t(sctp);
                    musicItem["download_time"] = std::to_string(download_time);
                    
                    // 延迟设置（从TaskManager中查找）
                    int delay_ms = 0;
                    {
                        std::lock_guard<std::mutex> lock(nt.mutex_);
                        for (const auto& task : nt.tasks_) {
                            if (task.second.file_send_name == custom_filename) {
                                delay_ms = task.second.delay_ms;
                                break;
                            }
                        }
                    }
                    musicItem["delay_ms"] = std::to_string(delay_ms);
                    
                    musicList.push_back(musicItem);
                }
            }
        }
        
        // 按下载时间倒序排序（最新的在前）
        std::sort(musicList.begin(), musicList.end(), 
                  [](const auto& a, const auto& b) {
                      return std::stoll(a.at("download_time")) > 
                             std::stoll(b.at("download_time"));
                  });
        
    } catch (const std::exception& e) {
        std::cerr << "扫描音乐库失败: " << e.what() << std::endl;
    }
    
    return musicList;
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

// 从查询字符串中获取参数值
std::string getQueryParam(std::string& query, const std::string& param) {
    size_t pos = query.find(param + "=");
    if (pos == std::string::npos) return "";
    
    std::string value = query.substr(pos + param.length() + 1);
    // 移除可能的后缀
    size_t end = value.find('&');
    if (end != std::string::npos) {
        value = value.substr(0, end);
    }
    // URL解码
    std::string decoded;
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%' && i + 2 < value.length()) {
            int hex;
            std::stringstream ss;
            ss << std::hex << value.substr(i + 1, 2);
            ss >> hex;
            decoded += static_cast<char>(hex);
            i += 2;
        } else if (value[i] == '+') {
            decoded += ' ';
        } else {
            decoded += value[i];
        }
    }
    return decoded;
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
                
                send(clientSocket, response.str().c_str(), response.str().length(), MSG_NOSIGNAL);
                
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
                return;
            }
            
            // 获取任务状态
            std::string status = TaskManager::instance().getTaskStatus(task_id);
            sendResponse(clientSocket, "200 OK", status);
            return;
        }

        // 文件下载请求
        if (request.getPath() == "/api/download/file") {
            std::string query = request.getQueryString();
            std::string task_id;
            std::string filename;
            
            // 尝试通过task_id查找
            task_id = getTaskIdByJson(query);
            
            // 如果task_id为空，尝试通过filename查找
            if (task_id.empty()) {
                filename = getQueryParam(query, "filename");
                if (filename.empty()) {
                    std::cerr << "找不到任务id或文件名" << std::endl;
                    sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"missing_task_id_or_filename\"}");                 
                    return;
                }
            }

            { 
                // 开始上锁防止下载时文件被删掉
                std::lock_guard<std::mutex> lock(nt.mutex_);
                
                std::string file_name_path = "";
                std::string display_filename = "";
                
                // 通过task_id查找文件
                if (!task_id.empty()) {
                    auto it = nt.tasks_.find(task_id);
                    if (it != nt.tasks_.end()) {
                        // 查看文件是否下载成功
                        if (!it->second.is_finished) {
                            sendResponse(clientSocket, "404 Need Wait Task Finish", "{\"error\":\"please wait task finish\"}");                         
                            return;
                        }
                        file_name_path = it->second.file_path_name;
                        display_filename = it->second.file_send_name;
                    }
                }
                // 通过filename查找文件
                else if (!filename.empty()) {
                    // 在任务列表中查找匹配的文件名
                    for (const auto& task : nt.tasks_) {
                        if (task.second.is_finished && 
                            (task.second.file_send_name == filename || 
                             task.second.file_path_name.find(filename) != std::string::npos)) {
                            file_name_path = task.second.file_path_name;
                            display_filename = task.second.file_send_name;
                            break;
                        }
                    }
                    
                    // 如果没找到，尝试直接查找下载目录中的文件
                    if (file_name_path.empty()) {
                        std::string downloadPath = downloadPath_;
                        std::string potential_path = downloadPath + filename;
                        MusicAnaly analy;
                        if (analy.fileExists(potential_path)) {
                            file_name_path = potential_path;
                            display_filename = filename;
                        }
                    }
                }

                    // 查看文件是否存在
                    MusicAnaly analy;
                    if (file_name_path.empty() || !analy.fileExists(file_name_path)) {
                        sendResponse(clientSocket, "404 File Not Found", "{\"error\":\"file missing\"}");                         
                        return;
                    }

                    // 发送文件
                    std::cout << "文件发送开始：" << file_name_path << std::endl;

                    // 读取文件内容
                    std::ifstream file(file_name_path, std::ios::binary);
                    if (!file.is_open()) {
                        sendResponse(clientSocket, "500 Internal Server Error", "{\"error\":\"cannot_open_file\"}");                        
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
                
                    // 获取显示文件名
                    if (display_filename.empty()) {
                        display_filename = fs::path(file_name_path).filename().string();
                    }
                    
                    // 如果是 文件名 - 作者 的形式则将作者去掉
                    size_t pos = display_filename.find(" - ");
                    if (pos != std::string::npos) {
                        display_filename = display_filename.substr(0, pos);
                        // 尝试获取文件扩展名
                        std::string ext = fs::path(file_name_path).extension().string();
                        if (!ext.empty()) {
                            display_filename += ext;
                        }
                    }

                    // 发送文件
                    std::stringstream response;
                    response << "HTTP/1.1 200 OK\r\n";
                    response << "Content-Type: application/octet-stream\r\n";
                    response << "Content-Disposition: attachment; filename=\"" << urlEncodeUtf8(display_filename) << "\"\r\n";
                    response << "Content-Length: " << fileSize << "\r\n";
                    response << "\r\n";
                
                    // 先发送头部
                    std::string headers = response.str();
                    send(clientSocket, headers.c_str(), headers.length(), MSG_NOSIGNAL);
                
                    // 然后发送文件内容
                    send(clientSocket, buffer.data(), buffer.size(), MSG_NOSIGNAL);
                
                    std::cout << "文件发送成功: " << display_filename << " (" << fileSize << " 字节)" << std::endl;
                    
                    // 标记文件正在被使用
                    if (!task_id.empty()) {
                        auto it = nt.tasks_.find(task_id);
                        if (it != nt.tasks_.end()) {
                            it->second.ifusing = true;
                        }
                    }                     
                    return;
            }
    
        }
        
        // 流式播放
        if (request.getPath() == "/api/download/stream") {
            
            std::string query_string = request.getQueryString();
            std::string task_id = getTaskIdByJson(query_string);
            std::string filename = getQueryParam(query_string, "filename");
            
            if (task_id.empty() && filename.empty()) {
                std::cerr << "找不到任务id或文件名" << std::endl;
                sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"missing_task_id_or_filename\"}");                 
                return;
            }

            // 查看文件是否还在
            {
                // 开始上锁防止下载时文件被删掉
                std::lock_guard<std::mutex> lock(nt.mutex_);
                
                std::string file_name_path = "";
                
                // 通过task_id查找文件
                if (!task_id.empty()) {
                    auto it = nt.tasks_.find(task_id);
                    if (it != nt.tasks_.end()) {
                        // 查看文件是否下载成功
                        if (!it->second.is_finished) {
                            sendResponse(clientSocket, "404 Need Wait Task Finish", "{\"error\":\"please wait task finish\"}");                         
                            return;
                        }
                        file_name_path = it->second.file_path_name;
                    }
                }
                // 通过filename查找文件
                else if (!filename.empty()) {
                    // 在任务列表中查找匹配的文件名
                    for (const auto& task : nt.tasks_) {
                        if (task.second.is_finished && 
                            (task.second.file_send_name == filename || 
                             task.second.file_path_name.find(filename) != std::string::npos)) {
                            file_name_path = task.second.file_path_name;
                            break;
                        }
                    }
                    
                    // 如果没找到，尝试直接查找下载目录中的文件
                    if (file_name_path.empty()) {
                        std::string downloadPath = downloadPath_;
                        std::string potential_path = downloadPath + filename;
                        MusicAnaly analy;
                        if (analy.fileExists(potential_path)) {
                            file_name_path = potential_path;
                        }
                    }
                }

                if (file_name_path.empty()) {
                    std::cerr << "文件被删除或还未被找到" << std::endl; 
                    sendResponse(clientSocket, "404 File Not Found", "{\"error\":\"file missing\"}");                    
                    return;
                }

                // 查看文件是否存在
                MusicAnaly analy;
                if (!analy.fileExists(file_name_path)) {
                    std::cerr << "流式传输音频失败，文件被删除" << std::endl;
                    sendResponse(clientSocket, "404 File Not Found", "{\"error\":\"file missing\"}");
                    return;
                }

                std::cout << "收到流式音频请求：" << file_name_path << std::endl;
                StreamSend strm(file_name_path, MAX_CHUNK_SIZE);
                bool status = strm.read_data_to_buffer(request.getRangeString());
                
                // 构建响应头
                std::stringstream response;
                if (status) {
                    response << "HTTP/1.1 206 Partial Content\r\n";
                    response << "Content-Range: bytes " << strm.file_buff_start_ << "-" << strm.file_buff_end_
                                << "/" << strm.file_size_ << "\r\n";
                } else {
                    response << "HTTP/1.1 200 OK\r\n";
                }

                response << "Content-Type: audio/mp4\r\n";
                response << "Content-Length: " << strm.request_size_ << "\r\n";
                response << "Accept-Ranges: bytes\r\n";
                response << "Connection: close\r\n";
                response << "Access-Control-Allow-Origin: *\r\n";
                response << "Cache-Control: no-cache, no-store, must-revalidate\r\n";
                response << "\r\n";

                std::string header = response.str();
                // 发送头部
                ssize_t sent = send(clientSocket, header.c_str(), header.length(), MSG_NOSIGNAL);
                if (sent != (ssize_t)header.length()) {
                    std::cerr << "发送头部失败，期望 " << header.length() 
                            << "，实际 " << sent << std::endl;                        
                    return;
                }

                // 标记文件正在被使用
                if (!task_id.empty()) {
                    auto it = nt.tasks_.find(task_id);
                    if (it != nt.tasks_.end()) {
                        it->second.ifusing = true;
                    }
                }
                
                // 发送数据
                struct timeval tv;
                tv.tv_sec = 30;   // 5秒超时
                tv.tv_usec = 0;
                if (setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, 
                            (const char*)&tv, sizeof(tv)) < 0) {
                    std::cerr << "设置发送超时失败: " << strerror(errno) << std::endl;
                }
                sent = send(clientSocket, strm.buffer_.data(), strm.request_size_, MSG_NOSIGNAL);
                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        std::cerr << "发送超时，数据未完全发送" << std::endl;
                    } else {
                        std::cerr << "发送失败: " << strerror(errno) << std::endl;
                    }
                } else if (sent != strm.request_size_) {
                    std::cerr << "发送数据不完整，期望 " << strm.request_size_ 
                              << "，实际 " << sent << std::endl;
                } else {
                    std::cout << "音频数据发送完成: " << sent << " 字节" << std::endl;
                }
                
                return;
            }

        }

        // 音乐库列表API
        if (request.getPath() == "/api/library/list") {
            try {
                // 扫描下载目录（使用配置的路径）
                std::string downloadPath = downloadPath_;
                
                // 扫描音乐文件
                auto musicList = scanMusicLibrary(downloadPath);
                
                // 构建JSON响应
                json responseJson = json::array();
                
                for (const auto& item : musicList) {
                    json musicJson;
                    musicJson["custom_filename"] = item.at("filename");
                    musicJson["system_filename"] = item.at("system_filename");
                    musicJson["download_time"] = std::stoll(item.at("download_time"));
                    musicJson["delay_ms"] = std::stoi(item.at("delay_ms"));
                    musicJson["file_size"] = std::stoll(item.at("file_size"));
                    responseJson.push_back(musicJson);
                }
                
                sendResponse(clientSocket, "200 OK", responseJson.dump());
                
            } catch (const std::exception& e) {
                json errorJson;
                errorJson["error"] = "Failed to scan music library: " + std::string(e.what());
                sendResponse(clientSocket, "500 Internal Server Error", errorJson.dump());
            }
            
            return;
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
                    return;
                }
            } catch (const json::parse_error& e) {
                sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"Invalid JSON format\"}");                 
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
                return;
            }
            
            // 创建下载任务 TODO
            std::string platform = "酷狗音乐";
            int offsetMs = 0;

            try {
                json j = json::parse(body);
                if (j.contains("platform") && j["platform"].is_string()) {
                    platform = j["platform"].get<std::string>();
                    if (debug) std::cout << "音乐平台: " << platform << std::endl;
                } else {
                    // 如果前端未发送，保持默认值
                    platform = "酷狗音乐";
                    if (debug) std::cout << "使用默认音乐平台: " << platform << std::endl;
                }
                
                // 解析offsetMs（delay参数）
                if (j.contains("offsetMs") && j["offsetMs"].is_number_integer()) {
                    offsetMs = j["offsetMs"].get<int>();
                    if (debug) std::cout << "Delay参数: " << offsetMs << "ms" << std::endl;
                } else {
                    // 如果前端未发送，保持默认值
                    offsetMs = 0;
                    if (debug) std::cout << "使用默认Delay参数: " << offsetMs << "ms" << std::endl;
                }
            }
            catch(const std::exception& e)
            {
                sendResponse(clientSocket, "400 Bad Request", "{\"error\":\"Invalid JSON format\"}");                 
                return;
            }
            

            std::string task_id = TaskManager::instance().createTask(content, file_name, platform, offsetMs);
            
            // 使用nlohmann/json创建JSON响应
            json responseJson;
            responseJson["task_id"] = task_id;
            responseJson["message"] = "download_started";
            responseJson["url"] = content;
            
            if(debug) std::cout << "创建任务id：" << task_id << " 链接：" << content << std::endl;
            
            sendResponse(clientSocket, "200 OK", responseJson.dump());             

        } else {
            sendResponse(clientSocket, "404 Not Found", "{\"error\": \"Not found\"}");
        }
                 
}

HttpServer::HttpServer(int port, const std::string& downloadPath) 
    : port_(port), serverSocket_(-1), downloadPath_(downloadPath) {}

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

    // 定时清理map表 - 保存线程对象避免泄漏
    pool_.detach_task([this]() {
        while (running_) { 
            TaskManager::instance().cleanupOldTasks(60);
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    });
    
    while (true) {
        sockaddr_in clientAddress;
        socklen_t clientLength = sizeof(clientAddress);
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddress, &clientLength);
        
        if (clientSocket < 0) {
            std::cerr << "接收错误！" << std::endl;
            continue;
        }
        
        pool_.detach_task([this, clientSocket]() {
            handleRequest(clientSocket);
            close(clientSocket);
        });

    }
}

HttpServer::~HttpServer() {
    
    running_ = false;  // 通知线程退出

    if (serverSocket_ >= 0) {
        close(serverSocket_);
        std::cout << "服务器已关闭" << std::endl;
    }
}