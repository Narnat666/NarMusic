#include "music_analysis.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <filesystem>

// CURL写回调函数（用于接收文本数据）
size_t MusicAnaly::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    CallbackData* data = static_cast<CallbackData*>(userp);
    data->response.append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// CURL写文件回调函数
size_t MusicAnaly::writeFileCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    CallbackData* data = static_cast<CallbackData*>(userp);
    size_t written = fwrite(contents, size, nmemb, data->outputFile);
    
    // 更新下载字节数
    if (data->downloadedBytes) {
        *data->downloadedBytes += written * size;
    }
    
    return written;
}

// CURL进度回调函数
int MusicAnaly::progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    MusicAnaly* analyzer = static_cast<MusicAnaly*>(clientp);
    
    // 如果设置了取消标志，返回非0值会中止传输
    if (analyzer->shouldCancel.load()) {
        return 1; // 非0值会中止传输
    }
    
    // 更新下载字节数
    analyzer->downloadedBytes.store(dlnow);
    
    // 返回0继续传输
    return 0;
}

// 构造函数
MusicAnaly::MusicAnaly(const std::string& filename, 
                      const std::string& filetype, 
                      const std::string& downloadpath)
    : outputFilename_(filename), 
      outputfiletype_(filetype), 
      outputfiledownloadpath_(downloadpath),
      curl(nullptr) {
    
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    // 确保下载路径存在
    if (!outputfiledownloadpath_.empty() && outputfiledownloadpath_ != "./") {
        std::filesystem::create_directories(outputfiledownloadpath_);
    }
}

// 析构函数
MusicAnaly::~MusicAnaly() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

// 从URL提取BVID
std::string MusicAnaly::getBVID(const std::string& url) {
    size_t bv_pos = url.find("BV");
    if (bv_pos != std::string::npos && bv_pos + 12 <= url.length()) {
        std::string bvid = url.substr(bv_pos, 12);
        // 验证BVID格式
        if (bvid.find("BV") == 0 && bvid.length() == 12) {
            return bvid;
        }
    }
    return "";
}

// 从JSON提取字符串
std::string MusicAnaly::extractJsonString(const json& j, const std::string& key) {
    try {
        if (j.contains(key) && !j[key].is_null()) {
            if (j[key].is_string()) {
                return j[key].get<std::string>();
            } else if (j[key].is_number()) {
                return std::to_string(j[key].get<long long>());
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON提取错误 (" << key << "): " << e.what() << std::endl;
    }
    return "";
}

// 从JSON提取整数
std::string MusicAnaly::extractJsonInt(const json& j, const std::string& key) {
    try {
        if (j.contains(key) && !j[key].is_null()) {
            if (j[key].is_number()) {
                return std::to_string(j[key].get<long long>());
            } else if (j[key].is_string()) {
                return j[key].get<std::string>();
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "JSON提取错误 (" << key << "): " << e.what() << std::endl;
    }
    return "";
}

// 获取JSON数据
bool MusicAnaly::fetchJsonData(const std::string& url, json& jsonData, const std::vector<std::string>& headers) {
    if (!curl) {
        std::cerr << "CURL未初始化" << std::endl;
        return false;
    }
    
    CallbackData data;
    data.response.clear();
    
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(curl, CURLOPT_REFERER, "https://www.bilibili.com");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // 添加自定义头部
    struct curl_slist* chunk = nullptr;
    for (const auto& header : headers) {
        chunk = curl_slist_append(chunk, header.c_str());
    }
    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }
    
    res = curl_easy_perform(curl);
    
    // 清理头部
    if (chunk) {
        curl_slist_free_all(chunk);
    }
    
    if (res != CURLE_OK) {
        std::cerr << "CURL请求失败: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    try {
        jsonData = json::parse(data.response);
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON解析失败: " << e.what() << std::endl;
        std::cerr << "响应内容: " << data.response.substr(0, 200) << "..." << std::endl;
        return false;
    }
}

// 下载文件
bool MusicAnaly::downloadFile(const std::string& url, const std::string& outputPath) {
    if (!curl) {
        std::cerr << "CURL未初始化" << std::endl;
        return false;
    }
    
    FILE* file = fopen(outputPath.c_str(), "wb");
    if (!file) {
        std::cerr << "无法创建文件: " << outputPath << std::endl;
        return false;
    }
    
    CallbackData data;
    data.downloadedBytes = &downloadedBytes;
    data.outputFile = file;
    
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(curl, CURLOPT_REFERER, "https://www.bilibili.com");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    
    // 设置超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // 无总超时
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L); // 1KB/s
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L); // 30秒
    
    std::cout << "开始下载: " << url << " -> " << outputPath << std::endl;
    
    res = curl_easy_perform(curl);
    fclose(file);
    
    if (res != CURLE_OK) {
        if (res == CURLE_ABORTED_BY_CALLBACK && shouldCancel.load()) {
            std::cout << "下载已被取消" << std::endl;
        } else {
            std::cerr << "下载失败: " << curl_easy_strerror(res) << std::endl;
        }
        // 删除可能已下载的部分文件
        std::filesystem::remove(outputPath);
        return false;
    }
    
    return true;
}

// 主下载函数
bool MusicAnaly::download(const std::string& url) {
    // 状态重置
    downloadedBytes = 0;
    isDownloading = true;
    downloadFinished = false;
    downloadSuccess = false;
    shouldCancel = false;
    
    std::cout << "处理URL: " << url << std::endl;
    
    // 提取BVID
    std::string bvid = getBVID(url);
    if (bvid.empty()) {
        std::cerr << "无法从URL提取BVID: " << url << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    std::cout << "提取到BVID: " << bvid << std::endl;
    
    // 获取视频信息
    std::string infoUrl = "https://api.bilibili.com/x/web-interface/view?bvid=" + bvid;
    json infoJson;
    
    if (!fetchJsonData(infoUrl, infoJson)) {
        std::cerr << "获取视频信息失败" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    
    // 提取aid和cid
    std::string aid, cid;
    try {
        if (infoJson.contains("data")) {
            const auto& data = infoJson["data"];
            aid = extractJsonString(data, "aid");
            cid = extractJsonString(data, "cid");
        }
    } catch (const json::exception& e) {
        std::cerr << "解析视频信息失败: " << e.what() << std::endl;
    }
    
    if (aid.empty() || cid.empty()) {
        std::cerr << "无法提取aid或cid" << std::endl;
        std::cerr << "响应内容: " << infoJson.dump(2) << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    std::cout << "提取到aid: " << aid << ", cid: " << cid << std::endl;
    
    // 获取音频信息
    std::string audioUrl = "https://api.bilibili.com/x/player/playurl?avid=" + aid + 
                          "&cid=" + cid + "&qn=0&type=&otype=json&fnver=0&fnval=80";
    json audioJson;
    
    if (!fetchJsonData(audioUrl, audioJson)) {
        std::cerr << "获取音频信息失败" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    
    // 提取音频下载URL
    std::string audioDownloadUrl;
    try {
        if (audioJson.contains("data") && audioJson["data"].contains("dash")) {
            const auto& dash = audioJson["data"]["dash"];
            if (dash.contains("audio") && dash["audio"].is_array() && !dash["audio"].empty()) {
                const auto& audio = dash["audio"][0];
                if (audio.contains("baseUrl")) {
                    audioDownloadUrl = audio["baseUrl"].get<std::string>();
                }
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "解析音频信息失败: " << e.what() << std::endl;
    }
    
    if (audioDownloadUrl.empty()) {
        std::cerr << "无法提取音频下载URL" << std::endl;
        std::cerr << "响应内容: " << audioJson.dump(2) << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    
    // 处理转义字符
    size_t pos;
    while ((pos = audioDownloadUrl.find("\\u0026")) != std::string::npos) {
        audioDownloadUrl.replace(pos, 6, "&");
    }
    
    std::cout << "提取到音频下载URL: " << audioDownloadUrl.substr(0, 100) << "..." << std::endl;
    
    // 构建输出文件名
    std::string outputFilename = outputFilename_;
    if (outputFilename.empty()) {
        outputFilename = bvid;
    }
    outputFilename += outputfiletype_;
    std::string outputPath = outputfiledownloadpath_ + outputFilename;
    
    // 如果文件已存在，删除它
    if (fileExists(outputPath)) {
        std::cout << "删除已存在的文件: " << outputPath << std::endl;
        std::filesystem::remove(outputPath);
    }
    
    // 启动下载线程
    std::thread download_thread([this, audioDownloadUrl, outputPath]() {
        bool success = this->downloadFile(audioDownloadUrl, outputPath);
        
        // 更新状态
        this->downloadSuccess = success;
        this->isDownloading = false;
        this->downloadFinished = true;
        
        if (success) {
            long long fileSize = getFileSize(outputPath);
            this->downloadedBytes = fileSize;
            std::cout << "下载完成，文件大小: " << fileSize << " 字节" << std::endl;
        }
    });
    
    download_thread.detach();
    
    return true;
}

// 取消下载
void MusicAnaly::cancelDownload() {
    shouldCancel = true;
}

// 获取下载字节数
long long MusicAnaly::getDownloadBytes() const {
    return downloadedBytes.load();
}

// 检查是否正在下载
bool MusicAnaly::ifDownloading() const {
    return isDownloading.load();
}

// 检查下载是否完成
bool MusicAnaly::downloadIfFinished() const {
    return downloadFinished.load();
}

// 检查下载是否成功
bool MusicAnaly::downloadIfSuccess() const {
    return downloadSuccess.load();
}

// 获取下载进度百分比
double MusicAnaly::getDownloadProgress() const {
    // 这里需要知道文件总大小才能计算百分比
    // 由于我们不知道总大小，所以返回-1表示未知
    // 或者可以实现更复杂的逻辑来获取总大小
    return -1.0; // 表示未知
}

// 设置输出文件名
void MusicAnaly::setOutputFilename(const std::string& filename) {
    outputFilename_ = filename;
}

// 设置输出文件类型
void MusicAnaly::setOutputFiletype(const std::string& filetype) {
    outputfiletype_ = filetype;
}

// 设置输出路径
void MusicAnaly::setOutputPath(const std::string& path) {
    outputfiledownloadpath_ = path;
    if (!outputfiledownloadpath_.empty() && outputfiledownloadpath_ != "./") {
        std::filesystem::create_directories(outputfiledownloadpath_);
    }
}

// 检查文件是否存在
bool MusicAnaly::fileExists(const std::string& filename) {
    return std::filesystem::exists(filename);
}

// 获取文件大小
long long MusicAnaly::getFileSize(const std::string& filename) {
    try {
        return std::filesystem::file_size(filename);
    } catch (...) {
        return 0;
    }
}

// 从URL获取文件名
std::string MusicAnaly::getFilenameFromUrl(const std::string& url) {
    size_t lastSlash = url.find_last_of("/");
    if (lastSlash != std::string::npos && lastSlash + 1 < url.length()) {
        std::string filename = url.substr(lastSlash + 1);
        
        // 移除查询参数
        size_t questionMark = filename.find("?");
        if (questionMark != std::string::npos) {
            filename = filename.substr(0, questionMark);
        }
        
        return filename;
    }
    return "";
}