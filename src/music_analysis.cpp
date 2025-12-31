#include "music_analysis.h"


const std::string CURL_COMMAND = "curl -s -k ";
const std::string USER_AGENT_HEADER = "-H \"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\" ";
const std::string BILIBILI_REFERER_HEADER = "-H \"Referer: https://www.bilibili.com\" ";
const std::string BILIBILI_VIEW_API_BASE = "\"https://api.bilibili.com/x/web-interface/view?bvid=";
const std::string BILIBILI_API_QUERY_PARM = "&qn=0&type=&otype=json&fnver=0&fnval=80\"";
const std::string BILIBILI_PLAYER_API_BASE = "\"https://api.bilibili.com/x/player/playurl?avid=";
const std::string BILIBILI_CID = "&cid=";
const std::string BILIBILI_DOWN_LOAD_CURL = "curl -L -s -k -o \"";
const std::string ORIGIN_HEADER = "-H \"Origin: https://www.bilibili.com\" ";
const std::string ACCEPT_HEADER = "-H \"Accept: application/json, text/plain, */*\" ";

std::string MusicAnaly::getBVID(const std::string& url) { // 参数为视频链接
    size_t bv_pos = url.find("BV");
    if (bv_pos != std::string::npos && bv_pos + 12 <= url.length()) {
        return url.substr(bv_pos, 12);
    }
    return "";   
}

std::string MusicAnaly::buildInfoCmd(const std::string& bvid) {
    return CURL_COMMAND + USER_AGENT_HEADER + ORIGIN_HEADER + ACCEPT_HEADER + 
            BILIBILI_REFERER_HEADER + BILIBILI_VIEW_API_BASE + bvid + "\"";
}

std::string MusicAnaly::buildAudioCmd(const std::string& aid, const std::string& cid) { // 用于获取音频信息
    return CURL_COMMAND + USER_AGENT_HEADER + 
        BILIBILI_REFERER_HEADER + BILIBILI_PLAYER_API_BASE + aid + BILIBILI_CID + cid +
        BILIBILI_API_QUERY_PARM;
}

std::string MusicAnaly::cmdHandle(const std::string& cmd) {  // 返回链接处理函数
    std::string result;
    char buffer[4096];
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    std::cout << "handle result: " << result << std::endl;
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

std::string MusicAnaly::extracinfo(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
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

std::string MusicAnaly::getaudiourl(const std::string& json){ // 提取音频url
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

std::string MusicAnaly::replaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

std::string MusicAnaly::buildDownloadCmd(const std::string& filename, const std::string& audio_url) { // 构建下载链接
    return BILIBILI_DOWN_LOAD_CURL + filename + "\" " +
        USER_AGENT_HEADER + BILIBILI_REFERER_HEADER + "\"" + audio_url + "\"";
}

bool MusicAnaly::fileExists(const std::string& filename) {
    std::string cmd = "test -f \"" + filename + "\" && echo \"yes\" || echo \"no\"";
    return (cmdHandle(cmd) == "yes");
}

long long MusicAnaly::getFileSize(const std::string& filename) {
    if (!fileExists(filename)) return 0;
    
    std::string cmd = "stat -c%s \"" + filename + "\" 2>/dev/null || wc -c < \"" + filename + "\" 2>/dev/null";
    std::string result = cmdHandle(cmd);
    
    if (!result.empty()) {
        try {
            return std::stoll(result);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

long long MusicAnaly::getDownloadBytes(void) {
    return downloadedBytes;
}

bool MusicAnaly::ifDownloading(void) { // 查看是否正在下载
    return isDownloading;
}

bool MusicAnaly::downloadIfFinished(void) { // 查看下载是否完成
    return downloadFinished;
}

bool MusicAnaly::downloadIfSuccess(void) { // 查看下载是否成功
    return downloadSuccess;
}

bool MusicAnaly::download(const std::string& url) { // 下载操作函数
    
    std::string outputFilename = outputFilename_;
    std::cout << "handle url: " << url << std::endl;

    // 状态重置
    downloadedBytes = 0;
    isDownloading = true;
    downloadFinished = false;
    downloadSuccess = false;

    // 根据视频链接提取bvid;
    std::string bvid = getBVID(url);
    if (bvid.empty()) {
        std::cerr << "failed get bvid !" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    std::cout << "get bvid: " << bvid << std::endl;

    // 构建链接提取视频信息
    std::cout << "build info: " << buildInfoCmd(bvid) << std::endl;
    std::string info_json = cmdHandle(buildInfoCmd(bvid));
    if (info_json.empty()) {
        std::cerr << "failed to get video info !" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }

    // 提取关键字
    std::string aid = extracinfo(info_json, "aid");
    std::string cid = extracinfo(info_json, "cid");
    if (aid.empty() || cid.empty()) {
        std::cerr << "failed to get key word !" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    std::cout << "get aid: " << aid << "get cid: " << cid << std::endl;

    // 构造链接获取音频信息
    std::string audio_json = cmdHandle(buildAudioCmd(aid, cid));
    if (audio_json.empty()) {
        std::cerr << "failed to get audio json !" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }

    // 提取音频下载链接
    std::string audio_url = getaudiourl(audio_json);
    if (audio_url.empty()) {
        std::cerr << "failed to get audio url !" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }

    // 构建下在文件名
    if (outputFilename.empty()) outputFilename = bvid;
    outputFilename += outputfiletype_;
    std::string download_file_path_name = outputfiledownloadpath_ + outputFilename;

    // 删掉旧文件
    std::string rm_cmd = "rm -f \"" + download_file_path_name + "\"";
    system(rm_cmd.c_str());

    // 下载开始
    std::cout << "begin down load: " << download_file_path_name << std::endl;
    std::string download_cmd = buildDownloadCmd(download_file_path_name, audio_url);
    std::thread download_thread([this, download_cmd, download_file_path_name]() { // 创建线程下载
        
        int result = system(download_cmd.c_str());
        // 更新状态
        downloadSuccess = (result == 0);
        isDownloading = false;
        downloadFinished = true;
        // 获取文件大小
        long long file_size = getFileSize(download_file_path_name);
        downloadedBytes = file_size;

    });

    // 实时更新下载进度
    std::thread monitor_thread([this, download_file_path_name]() {
        
        while (isDownloading) {
            long long current_size = getFileSize(download_file_path_name);
            downloadedBytes = current_size;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

    });

    download_thread.detach();
    monitor_thread.detach();

    return true; // 下载成功
}