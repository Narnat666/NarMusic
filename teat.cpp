#include <iostream>
#include <string>
#include <cstdlib>

class BilibiliAudioDownloader {
private:
    std::string execCommand(const std::string& cmd) {
        std::string result;
        char buffer[4096];
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::cerr << "执行命令失败!" << std::endl;
            return "";
        }
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
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
            while (end < json.length() && (isdigit(json[end]))) end++;
            return json.substr(start, end - start);
        }
        return "";
    }
    
    // 更健壮的音频URL提取，修复\u0026转义问题
    std::string extractAudioUrl(const std::string& json) {
        // 方案1：从 dash.audio[0].baseUrl 提取
        size_t audio_pos = json.find("\"audio\":[");
        if (audio_pos != std::string::npos) {
            size_t baseurl_pos = json.find("\"baseUrl\":\"", audio_pos);
            if (baseurl_pos != std::string::npos) {
                size_t url_start = baseurl_pos + 11;
                size_t url_end = json.find('"', url_start);
                if (url_end != std::string::npos) {
                    std::string url = json.substr(url_start, url_end - url_start);
                    // 修复JSON Unicode转义序列
                    url = replaceAll(url, "\\u0026", "&");
                    url = replaceAll(url, "\\\\u0026", "&"); // 双重转义情况
                    if (!url.empty() && url.find("http") == 0) {
                        return url;
                    }
                }
            }
        }
        
        // 方案2：尝试提取任何包含.m4s的baseUrl
        size_t baseurl_pos = json.find("\"baseUrl\":\"");
        while (baseurl_pos != std::string::npos) {
            size_t url_start = baseurl_pos + 11;
            size_t url_end = json.find('"', url_start);
            if (url_end != std::string::npos) {
                std::string url = json.substr(url_start, url_end - url_start);
                url = replaceAll(url, "\\u0026", "&");
                url = replaceAll(url, "\\\\u0026", "&");
                if (url.find(".m4s") != std::string::npos && url.find("http") == 0) {
                    return url;
                }
            }
            baseurl_pos = json.find("\"baseUrl\":\"", url_start);
        }
        
        return "";
    }

public:
    bool download(const std::string& url) {
        std::cout << "处理URL: " << url << std::endl;
        
        // 1. 提取BVID
        std::string bvid = extractBVID(url);
        if (bvid.empty()) {
            std::cerr << "错误: 无法提取BVID" << std::endl;
            return false;
        }
        std::cout << "BVID: " << bvid << std::endl;
        
        // 2. 获取aid和cid
        std::string info_cmd = "curl -s -k -H \"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\" "
                               "-H \"Referer: https://www.bilibili.com\" "
                               "\"https://api.bilibili.com/x/web-interface/view?bvid=" + bvid + "\"";
        std::string info_json = execCommand(info_cmd);
        if (info_json.empty()) {
            std::cerr << "错误: 获取视频信息失败" << std::endl;
            return false;
        }
        
        std::string aid = extractJsonField(info_json, "aid");
        std::string cid = extractJsonField(info_json, "cid");
        if (aid.empty() || cid.empty()) {
            std::cerr << "错误: 无法提取aid或cid" << std::endl;
            return false;
        }
        std::cout << "获取到 aid=" << aid << ", cid=" << cid << std::endl;
        
        // 3. 获取音频URL
        std::string audio_cmd = "curl -s -k -H \"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\" "
                                "-H \"Referer: https://www.bilibili.com\" "
                                "\"https://api.bilibili.com/x/player/playurl?avid=" + aid + 
                                "&cid=" + cid + "&qn=0&type=&otype=json&fnver=0&fnval=80\"";
        std::string audio_json = execCommand(audio_cmd);
        if (audio_json.empty()) {
            std::cerr << "错误: 获取音频信息失败" << std::endl;
            return false;
        }
        
        std::string audio_url = extractAudioUrl(audio_json);
        if (audio_url.empty()) {
            std::cerr << "错误: 无法提取音频URL" << std::endl;
            std::cerr << "响应片段: " << audio_json.substr(0, 300) << "..." << std::endl;
            return false;
        }
        std::cout << "音频URL: " << audio_url << std::endl;
        
        // 4. 下载音频
        std::string filename = bvid + ".m4a";
        std::cout << "开始下载音频到: " << filename << std::endl;
        
        // 先下载到临时文件
        std::string download_cmd = "curl -L --progress-bar -k "
                                   "-o \"" + filename + ".tmp\" "
                                   "-H \"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\" "
                                   "-H \"Referer: https://www.bilibili.com\" "
                                   "\"" + audio_url + "\"";
        
        int result = system(download_cmd.c_str());
        
        // 检查文件大小
        std::string check_cmd = "stat -c%s \"" + filename + ".tmp\" 2>/dev/null || wc -c < \"" + filename + ".tmp\"";
        std::string size_str = execCommand(check_cmd);
        int file_size = 0;
        if (!size_str.empty()) {
            file_size = std::stoi(size_str);
        }
        
        if (result == 0 && file_size > 1024) {
            // 重命名临时文件
            std::string mv_cmd = "mv \"" + filename + ".tmp\" \"" + filename + "\"";
            system(mv_cmd.c_str());
            std::cout << "下载成功: " << filename << " (" << file_size << " 字节)" << std::endl;
            return true;
        } else {
            std::cerr << "下载失败或文件过小 (" << file_size << " 字节)" << std::endl;
            // 清理临时文件
            std::string rm_cmd = "rm -f \"" + filename + ".tmp\"";
            system(rm_cmd.c_str());
            return false;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "B站音频下载器 (精简curl版)" << std::endl;
        std::cout << "===========================================" << std::endl;
        std::cout << "使用方法: " << argv[0] << " <B站视频URL>" << std::endl;
        std::cout << "示例: " << argv[0] << " https://www.bilibili.com/video/BV1KuB4B1E63" << std::endl;
        std::cout << "===========================================" << std::endl;
        return 1;
    }
    
    BilibiliAudioDownloader downloader;
    if (downloader.download(argv[1])) {
        std::cout << "===========================================" << std::endl;
        std::cout << "音频下载完成!" << std::endl;
        return 0;
    } else {
        std::cout << "===========================================" << std::endl;
        std::cerr << "音频下载失败!" << std::endl;
        return 1;
    }
}