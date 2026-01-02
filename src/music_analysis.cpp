#include "music_analysis.h"
#include "curl/curl.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <sys/stat.h>
#include <filesystem>

using json = nlohmann::json;
extern bool debug;

const std::string BILIBILI_VIEW_API_BASE = "https://api.bilibili.com/x/web-interface/view?bvid=";
const std::string BILIBILI_API_QUERY_PARM = "&qn=0&type=&otype=json&fnver=0&fnval=80";
const std::string BILIBILI_PLAYER_API_BASE = "https://api.bilibili.com/x/player/playurl?avid=";
const std::string BILIBILI_CID = "&cid=";

// curl回调函数用于接收数据
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// curl回调函数用于下载文件
static size_t WriteFileCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// curl进度回调函数
static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal;    // 明确标记为未使用
    (void)ultotal;    // 明确标记为未使用
    (void)ulnow;      // 明确标记为未使用
    long long* downloadedBytes = (long long*)clientp;
    if (downloadedBytes && dlnow > 0) {
        *downloadedBytes = dlnow;
    }
    return 0;
}

std::string MusicAnaly::getBVID(const std::string& url) {
    size_t bv_pos = url.find("BV");
    if (bv_pos != std::string::npos && bv_pos + 12 <= url.length()) {
        return url.substr(bv_pos, 12);
    }
    return "";   
}

// 使用curl库获取JSON数据
std::string MusicAnaly::fetchJsonData(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        headers = curl_slist_append(headers, "Referer: https://www.bilibili.com");
        headers = curl_slist_append(headers, "Origin: https://www.bilibili.com");
        headers = curl_slist_append(headers, "Accept: application/json, text/plain, */*");
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // 忽略SSL验证
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl函数执行失败：" << curl_easy_strerror(res) << std::endl;
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    return response;
}

// 使用nlohmann/json提取信息
std::string MusicAnaly::extractInfoFromJson(const std::string& jsonStr, const std::string& key) {
    try {
        json j = json::parse(jsonStr);
        
        // 递归查找key
        std::function<json(const json&, const std::string&)> findKey;
        findKey = [&](const json& obj, const std::string& k) -> json {
            if (obj.is_object()) {
                if (obj.contains(k)) {
                    return obj[k];
                }
                for (auto& [key, val] : obj.items()) {
                    if (val.is_object() || val.is_array()) {
                        json result = findKey(val, k);
                        if (!result.is_null()) {
                            return result;
                        }
                    }
                }
            } else if (obj.is_array()) {
                for (auto& item : obj) {
                    if (item.is_object() || item.is_array()) {
                        json result = findKey(item, k);
                        if (!result.is_null()) {
                            return result;
                        }
                    }
                }
            }
            return json();
        };
        
        json result = findKey(j, key);
        if (!result.is_null()) {
            if (result.is_string()) {
                return result.get<std::string>();
            } else if (result.is_number()) {
                return std::to_string(result.get<long long>());
            } else {
                return result.dump();
            }
        }
    } catch (const json::parse_error& e) {
        std::cerr << "JSON 解析错误：" << e.what() << std::endl;
    }
    return "";
}

// 使用nlohmann/json提取音频URL
std::string MusicAnaly::getAudioUrlFromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        // 查找音频URL
        std::function<std::string(const json&)> findAudioUrl;
        findAudioUrl = [&](const json& obj) -> std::string {
            if (obj.is_object()) {
                if (obj.contains("audio") && obj["audio"].is_array() && !obj["audio"].empty()) {
                    for (auto& audioItem : obj["audio"]) {
                        if (audioItem.is_object() && audioItem.contains("baseUrl") && 
                            audioItem["baseUrl"].is_string()) {
                            return audioItem["baseUrl"].get<std::string>();
                        }
                    }
                }
                if (obj.contains("dash") && obj["dash"].is_object() && 
                    obj["dash"].contains("audio") && obj["dash"]["audio"].is_array() &&
                    !obj["dash"]["audio"].empty()) {
                    for (auto& audioItem : obj["dash"]["audio"]) {
                        if (audioItem.is_object() && audioItem.contains("baseUrl") && 
                            audioItem["baseUrl"].is_string()) {
                            return audioItem["baseUrl"].get<std::string>();
                        }
                    }
                }
                for (auto& [key, val] : obj.items()) {
                    if (val.is_object() || val.is_array()) {
                        std::string result = findAudioUrl(val);
                        if (!result.empty()) {
                            return result;
                        }
                    }
                }
            } else if (obj.is_array()) {
                for (auto& item : obj) {
                    if (item.is_object() || item.is_array()) {
                        std::string result = findAudioUrl(item);
                        if (!result.empty()) {
                            return result;
                        }
                    }
                }
            }
            return "";
        };
        
        return findAudioUrl(j);
    } catch (const json::parse_error& e) {
        std::cerr << "JSON 解析错误：" << e.what() << std::endl;
    }
    return "";
}

bool MusicAnaly::fileExists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

long long MusicAnaly::getFileSize(const std::string& filename) {
    if (!fileExists(filename)) return 0;
    
    struct stat buffer;
    if (stat(filename.c_str(), &buffer) == 0) {
        return buffer.st_size;
    }
    return 0;
}

long long MusicAnaly::getDownloadBytes(void) {
    return downloadedBytes;
}

bool MusicAnaly::ifDownloading(void) {
    return isDownloading;
}

bool MusicAnaly::downloadIfFinished(void) {
    return downloadFinished;
}

bool MusicAnaly::downloadIfSuccess(void) {
    return downloadSuccess;
}

// 创建必要目录函数
bool prepareDownloadFile(const std::string& file_path_name) {
    try {
        std::filesystem::path file_path(file_path_name);
        std::filesystem::create_directories(file_path.parent_path());
        
        // 检查目录是否创建成功
        if (!std::filesystem::exists(file_path.parent_path())) {
            std::cerr << "目录创建失败: " << file_path.parent_path() << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "创建目录时出错: " << e.what() << std::endl;
        return false;
    }
}

bool MusicAnaly::download(const std::string& url) {
    std::string outputFilename = downloadFilename_;
    if (debug) std::cout << "处理链接：" << url << std::endl;

    // 状态重置
    downloadedBytes = 0;
    isDownloading = true;
    downloadFinished = false;
    downloadSuccess = false;

    // 根据视频链接提取bvid
    std::string bvid = getBVID(url);
    if (bvid.empty()) {
        std::cerr << "获取 bvid失败！" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    if(debug) std::cout << "获取 bvid: " << bvid << std::endl;

    // 获取视频信息
    std::string infoUrl = BILIBILI_VIEW_API_BASE + bvid;
    std::string info_json = fetchJsonData(infoUrl);
    
    if (info_json.empty()) {
        std::cerr << "获取视频信息失败！" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }

    // 提取关键字
    std::string aid = extractInfoFromJson(info_json, "aid");
    std::string cid = extractInfoFromJson(info_json, "cid");
    if (aid.empty() || cid.empty()) {
        std::cerr << "获取关键字失败！" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    if (debug) std::cout << "获取 aid: " << aid << " 获取 cid: " << cid << std::endl;

    // 获取音频信息
    std::string audioUrl = BILIBILI_PLAYER_API_BASE + aid + BILIBILI_CID + cid + BILIBILI_API_QUERY_PARM;
    std::string audio_json = fetchJsonData(audioUrl);
    
    if (audio_json.empty()) {
        std::cerr << "获取音频json失败！" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }

    // 提取音频下载链接
    std::string audio_url = getAudioUrlFromJson(audio_json);
    if (audio_url.empty()) {
        std::cerr << "获取音频下载链接失败！" << std::endl;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }

    // 构建下载文件名
    if (outputFilename.empty()) outputFilename = bvid;
    outputFilename += downloadFiletype_;
    std::string download_file_path_name = downloadFilepath_ + outputFilename;

    // 删掉旧文件
    if (fileExists(download_file_path_name)) {
        remove(download_file_path_name.c_str());
    }

    // 下载开始
    std::cout << "\n文件下载开始：" << download_file_path_name << std::endl;

    if (prepareDownloadFile(download_file_path_name)) {
        if (debug) std::cout << "文件下载目录创建成功" << std::endl;
    }
    else {
        std::cerr << "文件下载目录创建失败：" << download_file_path_name << std::endl;
        downloadSuccess = false;
        isDownloading = false;
        downloadFinished = true;
        return false;
    }
    
    std::thread download_thread([this, audio_url, download_file_path_name]() {
        CURL* curl = curl_easy_init();
        FILE* file = fopen(download_file_path_name.c_str(), "wb");
        
        if (curl && file) {
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
            headers = curl_slist_append(headers, "Referer: https://www.bilibili.com");
            
            curl_easy_setopt(curl, CURLOPT_URL, audio_url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &downloadedBytes);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            
            CURLcode res = curl_easy_perform(curl);
            
            fclose(file);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            downloadSuccess = (res == CURLE_OK);
            isDownloading = false;
            downloadFinished = true;
            
            if (downloadSuccess) {
                downloadedBytes = getFileSize(download_file_path_name);
            }
        } else {
            if (file) fclose(file);
            downloadSuccess = false;
            isDownloading = false;
            downloadFinished = true;
        }

        if (downloadSuccess) 
            std::cout << "文件下载成功：" << download_file_path_name << std::endl;
        else 
            std::cout << " 文件下载失败：" << download_file_path_name << std::endl;

    });

    download_thread.detach();

    return true;
}

std::string MusicAnaly::getDownloadFilePathName(void) { // 获取文件路径+名字+后缀
    return downloadFilepath_ + downloadFilename_ + downloadFiletype_;
}

std::string MusicAnaly::getDownloadFileType(void) {
    return downloadFiletype_;
}