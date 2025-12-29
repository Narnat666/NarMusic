#ifndef MUSIC_ANALYSIS_H
#define MUSIC_ANALYSIS_H

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>


// 创建音乐解析类
class MusicAnaly {

    private:
        std::atomic<long long> downloadedBytes{0};  // 返还下载进度
        std::atomic<bool> isDownloading{false}; // 下载状态标志位
        std::atomic<bool> downloadFinished{false};  // 下载成功标志位
        std::atomic<bool> downloadSuccess{false};   // 下载成功标志位 
        std::string outputFilename_; // 文件名
        std::string outputfiletype_; // 文件后缀
        std::string outputfiledownloadpath_; // 文件下载路径
        
        std::string getBVID(const std::string& url); // 视频ID提取
        std::string cmdHandle(const std::string& cmd); // 返回链接处理函数
        std::string extracinfo(const std::string& json, const std::string& key); // 关键字提取函数
        std::string getaudiourl(const std::string& json); // 提取音频url
        std::string buildInfoCmd(const std::string& bvid); // 根据视频id构建视频信息提取链接
        std::string buildAudioCmd(const std::string& aid, const std::string& cid); // 根据aid和cid构建音频信息链接
        std::string buildDownloadCmd(const std::string& filename, const std::string& audio_url); // 构建下载链接
        std::string replaceAll(std::string str, const std::string& from, const std::string& to); // 字符串替换函数
        bool fileExists(const std::string& filename); // 文件存在查看函数
        long long getFileSize(const std::string& filename); // 获取文件大小函数

        
    public:
        bool download(const std::string& url); // 下载操作函数
        long long getDownloadBytes(void); // 获取下载量
        bool ifDownloading(void); // 查看是否正在下载
        bool downloadIfFinished(void); // 查看下载是否完成
        bool downloadIfSuccess(void); // 查看下载是否成功

        explicit MusicAnaly(const std::string& filename = "", const std::string& filetype = ".m4a", const std::string& downloadpath = "./") 
        : outputFilename_(filename), outputfiletype_(filetype), outputfiledownloadpath_(downloadpath) {
        }

};

#endif