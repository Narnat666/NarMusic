#ifndef MUSIC_ANALYSIS_H
#define MUSIC_ANALYSIS_H

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include "../curl/curl.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// 回调函数数据结构
struct CallbackData {
    std::string response;
    std::atomic<long long>* downloadedBytes;;
    FILE* outputFile;
};

// 创建音乐解析类
class MusicAnaly {
private:
    std::atomic<long long> downloadedBytes{0};      // 下载进度
    std::atomic<bool> isDownloading{false};         // 下载状态标志位
    std::atomic<bool> downloadFinished{false};      // 下载完成标志位
    std::atomic<bool> downloadSuccess{false};       // 下载成功标志位
    std::atomic<bool> shouldCancel{false};          // 取消下载标志位
    
    std::string outputFilename_;                    // 文件名
    std::string outputfiletype_;                    // 文件后缀
    std::string outputfiledownloadpath_;            // 文件下载路径
    
    CURL* curl;                                     // CURL句柄
    
    // CURL回调函数
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t writeFileCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    
    // 内部方法
    std::string getBVID(const std::string& url);
    std::string extractJsonString(const json& j, const std::string& key);
    std::string extractJsonInt(const json& j, const std::string& key);
    bool fetchJsonData(const std::string& url, json& jsonData, const std::vector<std::string>& headers = {});
    bool downloadFile(const std::string& url, const std::string& outputPath);
    
public:
    // 构造函数和析构函数
    explicit MusicAnaly(const std::string& filename = "", 
                       const std::string& filetype = ".m4a", 
                       const std::string& downloadpath = "./");
    ~MusicAnaly();
    
    // 下载相关方法
    bool download(const std::string& url);
    void cancelDownload();
    
    // 状态查询方法
    long long getDownloadBytes() const;
    bool ifDownloading() const;
    bool downloadIfFinished() const;
    bool downloadIfSuccess() const;
    double getDownloadProgress() const;  // 新增：获取下载进度百分比
    
    // 设置方法
    void setOutputFilename(const std::string& filename);
    void setOutputFiletype(const std::string& filetype);
    void setOutputPath(const std::string& path);
    
    // 静态工具方法
    static bool fileExists(const std::string& filename);
    static long long getFileSize(const std::string& filename);
    static std::string getFilenameFromUrl(const std::string& url);
};

#endif