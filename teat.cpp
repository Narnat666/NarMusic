#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

class BilibiliAudioDownloader {
private:
    // 下载状态成员变量
    std::atomic<long long> downloadedBytes{0};
    std::atomic<long long> totalBytes{0};
    std::atomic<int> downloadPercentage{0};
    std::atomic<bool> isDownloading{false};
    std::atomic<bool> downloadFinished{false};
    std::atomic<bool> downloadSuccess{false};
    std::string outputFilename;
    
    std::string execCommand(const std::string& cmd) {
        std::string result;
        char buffer[4096];
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        return result;
    }
    
    std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return str;
    }
    
    std::string extractBVID(const std::string& url) {
        size_t bv_pos = url.find("BV");
        if (bv_pos != std::string::npos && bv_pos + 12 <= url.length()) {
            return url.substr(bv_pos, 12);
        }
        return "";
    }
    
    std::string extractJsonField(const std::string& json, const std::string& field) {
        std::string pattern = "\"" + field + "\":";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return "";
        
        pos += pattern.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) pos++;
        if (pos >= json.length()) return "";
        
        if (json[pos] == '"') {
            size_t start = pos + 1;
            size_t end = json.find('"', start);
            return (end != std::string::npos) ? json.substr(start, end - start) : "";
        } else if (isdigit(json[pos])) {
            size_t start = pos;
            size_t end = pos;
            while (end < json.length() && isdigit(json[end])) end++;
            return json.substr(start, end - start);
        }
        return "";
    }
    
    std::string extractAudioUrl(const std::string& json) {
        size_t audio_pos = json.find("\"audio\":[");
        if (audio_pos != std::string::npos) {
            size_t baseurl_pos = json.find("\"baseUrl\":\"", audio_pos);
            if (baseurl_pos != std::string::npos) {
                size_t url_start = baseurl_pos + 11;
                size_t url_end = json.find('"', url_start);
                if (url_end != std::string::npos) {
                    std::string url = json.substr(url_start, url_end - url_start);
                    url = replaceAll(url, "\\u0026", "&");
                    url = replaceAll(url, "\\\\u0026", "&");
                    if (!url.empty() && url.find("http") == 0) {
                        return url;
                    }
                }
            }
        }
        return "";
    }
    
    bool fileExists(const std::string& filename) {
        std::string cmd = "test -f \"" + filename + "\" && echo \"yes\" || echo \"no\"";
        return (execCommand(cmd) == "yes");
    }
    
    long long getFileSize(const std::string& filename) {
        if (!fileExists(filename)) return 0;
        
        std::string cmd = "stat -c%s \"" + filename + "\" 2>/dev/null || wc -c < \"" + filename + "\" 2>/dev/null";
        std::string result = execCommand(cmd);
        
        if (!result.empty()) {
            try {
                return std::stoll(result);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }
    
    long long tryGetRemoteFileSize(const std::string& url) {
        std::string cmd = "curl -L -s -k -I \"" + url + "\" 2>&1";
        std::string header_output = execCommand(cmd);
        
        if (!header_output.empty()) {
            std::string search_str = "content-length:";
            size_t pos = header_output.find(search_str);
            if (pos == std::string::npos) {
                search_str = "Content-Length:";
                pos = header_output.find(search_str);
            }
            
            if (pos != std::string::npos) {
                pos += search_str.length();
                while (pos < header_output.length() && !isdigit(header_output[pos])) pos++;
                
                size_t end_pos = pos;
                while (end_pos < header_output.length() && isdigit(header_output[end_pos])) end_pos++;
                
                if (pos < end_pos) {
                    try {
                        long long size = std::stoll(header_output.substr(pos, end_pos - pos));
                        if (size > 0) return size;
                    } catch (...) {}
                }
            }
        }
        return 0;
    }

public:
    // 获取下载状态的方法
    long long getDownloadedBytes() const { return downloadedBytes.load(); }
    long long getTotalBytes() const { return totalBytes.load(); }
    int getDownloadPercentage() const { return downloadPercentage.load(); }
    bool getIsDownloading() const { return isDownloading.load(); }
    bool getDownloadFinished() const { return downloadFinished.load(); }
    bool getDownloadSuccess() const { return downloadSuccess.load(); }
    std::string getOutputFilename() const { return outputFilename; }
    
    bool download(const std::string& url) {
        std::cout << "处理URL: " << url << std::endl;
        
        // 重置状态
        downloadedBytes = 0;
        totalBytes = 0;
        downloadPercentage = 0;
        isDownloading = true;
        downloadFinished = false;
        downloadSuccess = false;
        
        std::string bvid = extractBVID(url);
        if (bvid.empty()) {
            std::cerr << "错误: 无法提取BVID" << std::endl;
            isDownloading = false;
            downloadFinished = true;
            return false;
        }
        std::cout << "BVID: " << bvid << std::endl;
        
        std::string info_cmd = "curl -s -k -H \"User-Agent: Mozilla/5.0\" "
                               "-H \"Referer: https://www.bilibili.com\" "
                               "\"https://api.bilibili.com/x/web-interface/view?bvid=" + bvid + "\"";
        std::string info_json = execCommand(info_cmd);
        if (info_json.empty()) {
            std::cerr << "错误: 获取视频信息失败" << std::endl;
            isDownloading = false;
            downloadFinished = true;
            return false;
        }
        
        std::string aid = extractJsonField(info_json, "aid");
        std::string cid = extractJsonField(info_json, "cid");
        if (aid.empty() || cid.empty()) {
            std::cerr << "错误: 无法提取aid或cid" << std::endl;
            isDownloading = false;
            downloadFinished = true;
            return false;
        }
        std::cout << "获取到 aid=" << aid << ", cid=" << cid << std::endl;
        
        std::string audio_cmd = "curl -s -k -H \"User-Agent: Mozilla/5.0\" "
                                "-H \"Referer: https://www.bilibili.com\" "
                                "\"https://api.bilibili.com/x/player/playurl?avid=" + aid + 
                                "&cid=" + cid + "&qn=0&type=&otype=json&fnver=0&fnval=80\"";
        std::string audio_json = execCommand(audio_cmd);
        if (audio_json.empty()) {
            std::cerr << "错误: 获取音频信息失败" << std::endl;
            isDownloading = false;
            downloadFinished = true;
            return false;
        }
        
        std::string audio_url = extractAudioUrl(audio_json);
        if (audio_url.empty()) {
            std::cerr << "错误: 无法提取音频URL" << std::endl;
            isDownloading = false;
            downloadFinished = true;
            return false;
        }
        
        // 尝试获取文件大小
        totalBytes = tryGetRemoteFileSize(audio_url);
        if (totalBytes > 0) {
            std::cout << "文件大小: " << (totalBytes / 1024) << " KB" << std::endl;
        } else {
            std::cout << "文件大小: 未知 (B站CDN限制)" << std::endl;
        }
        
        outputFilename = bvid + ".m4a";
        std::string temp_filename = outputFilename;
        
        // 清理旧临时文件
        std::string rm_cmd = "rm -f \"" + temp_filename + "\"";
        system(rm_cmd.c_str());
        
        std::cout << "开始下载: " << outputFilename << std::endl;
        
        // 启动下载线程
        std::string download_cmd = "curl -L -s -k -o \"" + temp_filename + "\" "
                                   "-H \"User-Agent: Mozilla/5.0\" "
                                   "-H \"Referer: https://www.bilibili.com\" \"" + audio_url + "\"";
        
        std::thread download_thread([this, download_cmd, temp_filename]() {
            int result = system(download_cmd.c_str());
            
            // 下载完成后更新状态
            downloadSuccess = (result == 0);
            isDownloading = false;
            downloadFinished = true;
            
            // 最终文件检查
            long long file_size = getFileSize(temp_filename);
            downloadedBytes = file_size;
            
            if (downloadSuccess && file_size > 1024) {
                std::string mv_cmd = "mv \"" + temp_filename + "\" \"" + outputFilename + "\"";
                system(mv_cmd.c_str());
            } else {
                std::string rm_cmd = "rm -f \"" + temp_filename + "\"";
                system(rm_cmd.c_str());
                downloadSuccess = false;
            }
        });
        
        // 监控线程 - 实时更新下载进度
        std::thread monitor_thread([this, temp_filename]() {
            while (isDownloading) {
                long long current_size = getFileSize(temp_filename);
                downloadedBytes = current_size;
                
                if (totalBytes > 0) {
                    int percentage = static_cast<int>((current_size * 100) / totalBytes);
                    downloadPercentage = std::min(percentage, 100);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        });
        
        download_thread.detach();
        monitor_thread.detach();
        
        return true; // 启动成功
    }
};

// 简单的进度显示函数
void displaySimpleProgress(long long downloaded, long long total) {
    std::cout << "\r已下载: " << (downloaded / 1024) << " KB";
    if (total > 0) {
        std::cout << " / " << (total / 1024) << " KB";
    }
    std::cout.flush();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "B站音频下载器" << std::endl;
        std::cout << "===========================================" << std::endl;
        std::cout << "使用方法: " << argv[0] << " <B站视频URL>" << std::endl;
        std::cout << "示例: " << argv[0] << " https://www.bilibili.com/video/BV1KuB4B1E63" << std::endl;
        std::cout << "===========================================" << std::endl;
        return 1;
    }
    
    BilibiliAudioDownloader downloader;
    
    // 启动下载
    if (!downloader.download(argv[1])) {
        std::cerr << "启动下载失败!" << std::endl;
        return 1;
    }
    
    // 主循环：轮询下载状态并显示
    while (!downloader.getDownloadFinished()) {
        displaySimpleProgress(
            downloader.getDownloadedBytes(),
            downloader.getTotalBytes()
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 显示最终状态
    std::cout << std::endl;
    
    if (downloader.getDownloadSuccess()) {
        std::cout << "===========================================" << std::endl;
        std::cout << "下载完成: " << downloader.getOutputFilename() << std::endl;
        std::cout << "文件大小: " << (downloader.getDownloadedBytes() / 1024) << " KB" << std::endl;
        return 0;
    } else {
        std::cout << "===========================================" << std::endl;
        std::cerr << "下载失败!" << std::endl;
        return 1;
    }
}