#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "taglib/mp4/mp4file.h"
#include "taglib/mp4/mp4tag.h"
#include "taglib/mp4/mp4coverart.h"
#include "taglib/mp4/mp4item.h"
#include "taglib/toolkit/tpropertymap.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <thread>
#include <future>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <memory>
#include <chrono>
#include <random>

using json = nlohmann::json;

class MusicMetadataUpdater {
private:
    struct MusicData {
        std::string source;
        std::string songId;
        std::string songName;
        std::string artist;
        std::string album;
        std::string lyrics;
        std::vector<uint8_t> coverData;
        std::string coverUrl;
        bool hasLyrics = false;
        bool hasCover = false;
        bool hasAlbum = false;
    };

    static size_t WriteStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        ((std::string*)userp)->append((char*)contents, total_size);
        return total_size;
    }

    static size_t WriteBinaryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(userp);
        const uint8_t* data = static_cast<const uint8_t*>(contents);
        buffer->insert(buffer->end(), data, data + total_size);
        return total_size;
    }

    bool performHttpRequest(const std::string& url, 
                           std::string& response,
                           const std::vector<std::string>& extraHeaders = {},
                           const std::string& postData = "",
                           long timeout = 15) {
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        headers = curl_slist_append(headers, "Accept: application/json, text/plain, */*");
        headers = curl_slist_append(headers, "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
        
        for (const auto& header : extraHeaders) {
            headers = curl_slist_append(headers, header.c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStringCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        if (!postData.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        }
        
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "HTTP请求失败: " << curl_easy_strerror(res) << " URL: " << url << std::endl;
            return false;
        }
        
        return true;
    }
    
    std::string encodeUrl(const std::string& value) {
        CURL* curl = curl_easy_init();
        if (!curl) return value;
        
        char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
        std::string result(encoded ? encoded : "");
        curl_free(encoded);
        curl_easy_cleanup(curl);
        
        return result;
    }
    
    std::string base64Decode(const std::string& encoded) {
        const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        
        std::string decoded;
        int val = 0;
        int bits = -8;
        
        for (unsigned char c : encoded) {
            if (c == '=') break;
            if (std::isspace(c)) continue;
            
            size_t index = base64_chars.find(c);
            if (index == std::string::npos) break;
            
            val = (val << 6) + static_cast<int>(index);
            bits += 6;
            
            if (bits >= 0) {
                decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
                bits -= 8;
            }
        }
        
        return decoded;
    }
    
    // 网易云音乐搜索 - 使用更稳定的API
    bool searchSongFromNetease(const std::string& keyword, MusicData& data) {
        std::string url = "http://music.163.com/api/cloudsearch/pc?s=" + 
                         encodeUrl(keyword) + "&type=1&offset=0&limit=3";
        
        std::string response;
        std::vector<std::string> headers = {
            "Referer: http://music.163.com",
            "Origin: http://music.163.com"
        };
        
        if (!performHttpRequest(url, response, headers)) {
            return false;
        }
        
        try {
            json j = json::parse(response);
            if (j.contains("code") && j["code"].get<int>() == 200 &&
                j.contains("result") && 
                j["result"].contains("songs") && 
                !j["result"]["songs"].empty()) {
                
                auto& song = j["result"]["songs"][0];
                data.songId = std::to_string(song["id"].get<long long>());
                
                if (song.contains("name")) {
                    data.songName = song["name"].get<std::string>();
                }
                
                if (song.contains("ar") && !song["ar"].empty() && song["ar"][0].contains("name")) {
                    data.artist = song["ar"][0]["name"].get<std::string>();
                }
                
                if (song.contains("al") && song["al"].contains("name")) {
                    data.album = song["al"]["name"].get<std::string>();
                    data.hasAlbum = true;
                }
                
                // 获取专辑封面
                if (song.contains("al") && song["al"].contains("picUrl")) {
                    data.coverUrl = song["al"]["picUrl"].get<std::string>();
                    // 移除参数，添加高质量参数
                    size_t pos = data.coverUrl.find("?");
                    if (pos != std::string::npos) {
                        data.coverUrl = data.coverUrl.substr(0, pos);
                    }
                    // 使用500x500的高质量封面
                    data.coverUrl += "?param=500y500";
                }
                
                return true;
            }
        } catch (const json::exception& e) {
            std::cerr << "网易云搜索JSON解析错误: " << e.what() << std::endl;
        }
        
        return false;
    }
    
    bool getLyricsFromNetease(MusicData& data) {
        std::string url = "http://music.163.com/api/song/lyric?id=" + data.songId + "&lv=-1&kv=-1&tv=-1";
        std::string response;
        
        std::vector<std::string> headers = {
            "Referer: http://music.163.com",
            "Origin: http://music.163.com"
        };
        
        if (!performHttpRequest(url, response, headers)) {
            return false;
        }
        
        try {
            json j = json::parse(response);
            if (j.contains("lrc") && j["lrc"].contains("lyric") && !j["lrc"]["lyric"].get<std::string>().empty()) {
                data.lyrics = j["lrc"]["lyric"].get<std::string>();
                data.hasLyrics = true;
                return true;
            }
        } catch (const json::exception& e) {
            std::cerr << "网易云歌词JSON解析错误: " << e.what() << std::endl;
        }
        
        return false;
    }
    
    // 酷狗音乐搜索 - 使用更简单的API
    bool searchSongFromKugou(const std::string& keyword, MusicData& data) {
        // 使用HTTP API，避免SSL问题
        std::string url = "http://mobilecdn.kugou.com/api/v3/search/song?format=json&keyword=" +
                         encodeUrl(keyword) + "&page=1&pagesize=5&showtype=1";
        
        std::string response;
        if (!performHttpRequest(url, response)) {
            return false;
        }
        
        try {
            json j = json::parse(response);
            if (j.contains("status") && j["status"].get<int>() == 1 &&
                j.contains("data") && 
                j["data"].contains("info") && 
                !j["data"]["info"].empty()) {
                
                auto& song = j["data"]["info"][0];
                
                // 获取FileHash作为歌曲ID
                if (song.contains("hash")) {
                    data.songId = song["hash"].get<std::string>();
                }
                
                if (song.contains("songname")) {
                    data.songName = song["songname"].get<std::string>();
                }
                
                if (song.contains("singername")) {
                    data.artist = song["singername"].get<std::string>();
                }
                
                if (song.contains("album_name")) {
                    data.album = song["album_name"].get<std::string>();
                    data.hasAlbum = true;
                }
                
                // 构建封面URL
                if (song.contains("album_id") && !song["album_id"].is_null()) {
                    std::string albumId = song["album_id"].get<std::string>();
                    if (!albumId.empty() && albumId != "0") {
                        // 简单的封面URL格式
                        data.coverUrl = "http://imgessl.kugou.com/stdmusic/" + albumId + ".jpg";
                    }
                }
                
                return true;
            }
        } catch (const json::exception& e) {
            std::cerr << "酷狗搜索JSON解析错误: " << e.what() << std::endl;
        }
        
        return false;
    }
    
    bool getLyricsFromKugou(MusicData& data) {
        if (data.songId.empty()) {
            return false;
        }
        
        // 使用hash获取歌词
        std::string url = "http://lyrics.kugou.com/search?ver=1&man=yes&client=pc&keyword=" + 
                         encodeUrl(data.songId) + "&duration=0";
        
        std::string response;
        if (!performHttpRequest(url, response)) {
            return false;
        }
        
        try {
            json j = json::parse(response);
            if (j.contains("candidates") && !j["candidates"].empty()) {
                auto& candidate = j["candidates"][0];
                
                std::string id = candidate["id"].get<std::string>();
                std::string accesskey = candidate["accesskey"].get<std::string>();
                
                // 下载歌词
                std::string downloadUrl = "http://lyrics.kugou.com/download?ver=1&client=pc&id=" +
                                        id + "&accesskey=" + accesskey + "&fmt=lrc&charset=utf8";
                
                std::string lyricResponse;
                if (performHttpRequest(downloadUrl, lyricResponse)) {
                    json lyricData = json::parse(lyricResponse);
                    if (lyricData.contains("content")) {
                        std::string base64Content = lyricData["content"].get<std::string>();
                        data.lyrics = base64Decode(base64Content);
                        data.hasLyrics = !data.lyrics.empty();
                        return data.hasLyrics;
                    }
                }
            }
        } catch (const json::exception& e) {
            std::cerr << "酷狗歌词JSON解析错误: " << e.what() << std::endl;
        }
        
        return false;
    }
    
    // QQ音乐搜索 - 使用简单的搜索API
    bool searchSongFromQQMusic(const std::string& keyword, MusicData& data) {
        std::string url = "https://c.y.qq.com/soso/fcgi-bin/client_search_cp?format=json&w=" +
                         encodeUrl(keyword) + "&n=5&p=1";
        
        std::string response;
        std::vector<std::string> headers = {
            "Referer: https://y.qq.com",
            "Origin: https://y.qq.com"
        };
        
        if (!performHttpRequest(url, response, headers)) {
            return false;
        }
        
        // 处理JSONP响应
        size_t start = response.find("callback(");
        if (start != std::string::npos) {
            size_t end = response.rfind(")");
            if (end != std::string::npos && end > start) {
                response = response.substr(start + 9, end - start - 9);
            }
        }
        
        try {
            json j = json::parse(response);
            if (j.contains("data") && 
                j["data"].contains("song") &&
                j["data"]["song"].contains("list") &&
                !j["data"]["song"]["list"].empty()) {
                
                auto& song = j["data"]["song"]["list"][0];
                data.songId = song["songmid"].get<std::string>();
                
                if (song.contains("songname")) {
                    data.songName = song["songname"].get<std::string>();
                }
                
                if (song.contains("singer") && !song["singer"].empty()) {
                    data.artist = song["singer"][0]["name"].get<std::string>();
                }
                
                if (song.contains("albumname")) {
                    data.album = song["albumname"].get<std::string>();
                    data.hasAlbum = true;
                }
                
                // 获取专辑封面
                if (song.contains("albummid")) {
                    std::string albumMid = song["albummid"].get<std::string>();
                    if (!albumMid.empty()) {
                        data.coverUrl = "https://y.gtimg.cn/music/photo_new/T002R500x500M000" + 
                                       albumMid + ".jpg";
                    }
                }
                
                return true;
            }
        } catch (const json::exception& e) {
            std::cerr << "QQ音乐搜索JSON解析错误: " << e.what() << std::endl;
        }
        
        return false;
    }
    
    bool getLyricsFromQQMusic(MusicData& data) {
        std::string url = "https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg?format=json&songmid=" +
                         data.songId + "&g_tk=5381";
        
        std::string response;
        std::vector<std::string> headers = {
            "Referer: https://y.qq.com",
            "Origin: https://y.qq.com"
        };
        
        if (!performHttpRequest(url, response, headers)) {
            return false;
        }
        
        // 处理JSONP响应
        if (response.find("MusicJsonCallback(") == 0) {
            size_t start = response.find('(');
            size_t end = response.rfind(')');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                response = response.substr(start + 1, end - start - 1);
            }
        }
        
        try {
            json j = json::parse(response);
            if (j.contains("lyric")) {
                std::string base64Lyric = j["lyric"].get<std::string>();
                data.lyrics = base64Decode(base64Lyric);
                data.hasLyrics = !data.lyrics.empty();
                return data.hasLyrics;
            }
        } catch (const json::exception& e) {
            std::cerr << "QQ音乐歌词JSON解析错误: " << e.what() << std::endl;
        }
        
        return false;
    }
    
    bool downloadImage(MusicData& data) {
        if (data.coverUrl.empty()) {
            return false;
        }
        
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        curl_easy_setopt(curl, CURLOPT_URL, data.coverUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBinaryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data.coverData);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK && !data.coverData.empty()) {
            data.hasCover = true;
            return true;
        }
        
        return false;
    }
    
    // 清理歌词，转换为同步歌词格式
    std::string cleanLyrics(const std::string& lyrics) {
        if (lyrics.empty()) return "";
        
        std::string cleaned = lyrics;
        
        // 移除UTF-8 BOM
        if (cleaned.size() >= 3 && 
            static_cast<unsigned char>(cleaned[0]) == 0xEF &&
            static_cast<unsigned char>(cleaned[1]) == 0xBB &&
            static_cast<unsigned char>(cleaned[2]) == 0xBF) {
            cleaned = cleaned.substr(3);
        }
        
        // 检查是否是LRC格式（包含时间标签）
        if (cleaned.find('[') != std::string::npos && 
            cleaned.find(']') != std::string::npos &&
            cleaned.find(':') != std::string::npos) {
            // LRC歌词，转换为标准格式
            return convertLRCToStandardFormat(cleaned);
        } else {
            // 纯文本歌词，移除HTML标签和多余空白
            cleaned = std::regex_replace(cleaned, std::regex("<[^>]*>"), "");
            cleaned = std::regex_replace(cleaned, std::regex("\\s+"), " ");
            cleaned = std::regex_replace(cleaned, std::regex("\\n\\s*\\n"), "\n");
            cleaned = std::regex_replace(cleaned, std::regex("^\\s+|\\s+$"), "");
            return cleaned;
        }
    }
    
    std::string convertLRCToStandardFormat(const std::string& lrcText) {
        std::istringstream stream(lrcText);
        std::string line;
        std::vector<std::pair<int, std::string>> lyrics;
        
        std::regex timeRegex(R"(\[(\d+):(\d+)(?:\.(\d+))?\])");
        
        while (std::getline(stream, line)) {
            std::smatch match;
            std::string::const_iterator searchStart(line.cbegin());
            
            // 查找所有时间标签
            std::vector<int> times;
            while (std::regex_search(searchStart, line.cend(), match, timeRegex)) {
                int minutes = std::stoi(match[1]);
                int seconds = std::stoi(match[2]);
                int milliseconds = 0;
                
                if (match[3].matched) {
                    std::string msStr = match[3];
                    if (msStr.length() == 2) {
                        milliseconds = std::stoi(msStr) * 10;
                    } else if (msStr.length() >= 3) {
                        milliseconds = std::stoi(msStr.substr(0, 3));
                    }
                }
                
                int totalMs = minutes * 60000 + seconds * 1000 + milliseconds;
                times.push_back(totalMs);
                searchStart = match.suffix().first;
            }
            
            // 获取歌词文本
            std::string lyricText = std::regex_replace(line, timeRegex, "");
            lyricText = std::regex_replace(lyricText, std::regex("^\\s+|\\s+$"), "");
            
            if (!lyricText.empty() && !times.empty()) {
                for (int time : times) {
                    lyrics.push_back(std::make_pair(time, lyricText));
                }
            }
        }
        
        // 按时间排序
        std::sort(lyrics.begin(), lyrics.end(),
                 [](const std::pair<int, std::string>& a, 
                    const std::pair<int, std::string>& b) { 
                     return a.first < b.first; 
                 });
        
        // 生成标准LRC格式
        std::stringstream result;
        for (const auto& entry : lyrics) {
            int time = entry.first;
            const std::string& text = entry.second;
            result << "[" 
                   << std::setfill('0') << std::setw(2) << (time / 60000) << ":"
                   << std::setfill('0') << std::setw(2) << ((time % 60000) / 1000) << "."
                   << std::setfill('0') << std::setw(3) << (time % 1000) << "]"
                   << text << "\n";
        }
        
        return result.str();
    }
    
    bool writeToM4AFile(const std::string& filepath,
                       const MusicData& data) {
        std::cout << "\n写入M4A文件: " << filepath << std::endl;
        
        TagLib::MP4::File file(filepath.c_str());
        if (!file.isOpen()) {
            std::cerr << "错误: 无法打开文件: " << filepath << std::endl;
            return false;
        }
        
        TagLib::MP4::Tag* tag = file.tag();
        if (!tag) {
            std::cerr << "错误: 无法获取文件标签" << std::endl;
            return false;
        }
        
        // 设置标题
        if (!data.songName.empty()) {
            tag->setTitle(TagLib::String(data.songName, TagLib::String::UTF8));
            std::cout << "标题: " << data.songName << std::endl;
        }
        
        // 设置艺术家
        if (!data.artist.empty()) {
            tag->setArtist(TagLib::String(data.artist, TagLib::String::UTF8));
            std::cout << "艺术家: " << data.artist << std::endl;
        }
        
        // 设置专辑
        if (data.hasAlbum && !data.album.empty()) {
            tag->setAlbum(TagLib::String(data.album, TagLib::String::UTF8));
            std::cout << "专辑: " << data.album << std::endl;
        } else if (!data.songName.empty()) {
            // 如果没有专辑信息，使用歌曲名作为专辑
            tag->setAlbum(TagLib::String(data.songName + " - Single", TagLib::String::UTF8));
        }
        
        // 写入封面
        if (data.hasCover && !data.coverData.empty()) {
            try {
                TagLib::MP4::CoverArt::Format format = TagLib::MP4::CoverArt::JPEG;
                
                // 检测图片格式
                if (data.coverData.size() > 8) {
                    // PNG格式: 89 50 4E 47
                    if (data.coverData[0] == 0x89 && data.coverData[1] == 0x50 && 
                        data.coverData[2] == 0x4E && data.coverData[3] == 0x47) {
                        format = TagLib::MP4::CoverArt::PNG;
                    }
                    // JPEG格式: FF D8
                    else if (data.coverData[0] == 0xFF && data.coverData[1] == 0xD8) {
                        format = TagLib::MP4::CoverArt::JPEG;
                    }
                }
                
                TagLib::MP4::CoverArt coverArt(
                    format,
                    TagLib::ByteVector(reinterpret_cast<const char*>(data.coverData.data()), 
                                      data.coverData.size())
                );
                
                TagLib::MP4::CoverArtList coverList;
                coverList.append(coverArt);
                
                TagLib::MP4::Item coverItem(coverList);
                tag->setItem("covr", coverItem);
                std::cout << "封面: 写入成功 (" << data.coverData.size() << " 字节)" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "封面写入错误: " << e.what() << std::endl;
            }
        } else {
            std::cout << "封面: 无封面数据" << std::endl;
        }
        
        // 写入歌词
        if (data.hasLyrics && !data.lyrics.empty()) {
            try {
                std::string cleanedLyrics = cleanLyrics(data.lyrics);
                TagLib::String lyricsStr(cleanedLyrics, TagLib::String::UTF8);
                
                // 写入标准歌词标签
                tag->setItem("\xA9lyr", TagLib::MP4::Item(lyricsStr));
                
                // 如果是同步歌词，设置额外标记
                if (cleanedLyrics.find('[') != std::string::npos && 
                    cleanedLyrics.find(']') != std::string::npos) {
                    // 同步歌词
                    tag->setItem("----:com.apple.iTunes:Lyrics Type", 
                                TagLib::MP4::Item(TagLib::String("Synced")));
                    std::cout << "歌词: 同步歌词 (" << cleanedLyrics.length() << " 字符)" << std::endl;
                } else {
                    std::cout << "歌词: 纯文本歌词 (" << cleanedLyrics.length() << " 字符)" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "歌词写入错误: " << e.what() << std::endl;
            }
        } else {
            std::cout << "歌词: 无歌词数据" << std::endl;
        }
        
        // 设置音轨和光盘编号
        tag->setItem("trkn", TagLib::MP4::Item(1, 1));
        tag->setItem("disk", TagLib::MP4::Item(1, 1));
        
        // 设置年份
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm* local_time = std::localtime(&now_time);
        int year = local_time->tm_year + 1900;
        tag->setItem("\xA9""day", TagLib::MP4::Item(TagLib::String(std::to_string(year))));
        
        // 保存文件
        bool saved = file.save();
        if (saved) {
            std::cout << "\n文件保存成功!" << std::endl;
            std::cout << "数据源: " << data.source << std::endl;
        } else {
            std::cerr << "错误: 保存文件失败" << std::endl;
        }
        
        return saved;
    }
    
    // 从网易云获取完整数据
    bool fetchFromNetease(const std::string& keyword, MusicData& data) {
        std::cout << "正在尝试网易云音乐..." << std::endl;
        
        data.source = "网易云音乐";
        
        if (!searchSongFromNetease(keyword, data)) {
            std::cout << "网易云: 搜索失败" << std::endl;
            return false;
        }
        
        // 获取歌词
        if (!getLyricsFromNetease(data)) {
            std::cout << "网易云: 获取歌词失败" << std::endl;
        }
        
        // 下载封面
        if (!data.coverUrl.empty()) {
            if (!downloadImage(data)) {
                std::cout << "网易云: 封面下载失败" << std::endl;
            }
        } else {
            std::cout << "网易云: 无封面URL" << std::endl;
        }
        
        if (data.hasLyrics || data.hasCover) {
            std::cout << "网易云: 获取成功" << std::endl;
            return true;
        }
        
        return false;
    }
    
    // 从酷狗获取完整数据
    bool fetchFromKugou(const std::string& keyword, MusicData& data) {
        std::cout << "正在尝试酷狗音乐..." << std::endl;
        
        data.source = "酷狗音乐";
        
        if (!searchSongFromKugou(keyword, data)) {
            std::cout << "酷狗: 搜索失败" << std::endl;
            return false;
        }
        
        // 获取歌词
        if (!getLyricsFromKugou(data)) {
            std::cout << "酷狗: 获取歌词失败" << std::endl;
        }
        
        // 下载封面
        if (!data.coverUrl.empty()) {
            if (!downloadImage(data)) {
                std::cout << "酷狗: 封面下载失败" << std::endl;
            }
        } else {
            std::cout << "酷狗: 无封面URL" << std::endl;
        }
        
        if (data.hasLyrics || data.hasCover) {
            std::cout << "酷狗: 获取成功" << std::endl;
            return true;
        }
        
        return false;
    }
    
    // 从QQ音乐获取完整数据
    bool fetchFromQQMusic(const std::string& keyword, MusicData& data) {
        std::cout << "正在尝试QQ音乐..." << std::endl;
        
        data.source = "QQ音乐";
        
        if (!searchSongFromQQMusic(keyword, data)) {
            std::cout << "QQ音乐: 搜索失败" << std::endl;
            return false;
        }
        
        // 获取歌词
        if (!getLyricsFromQQMusic(data)) {
            std::cout << "QQ音乐: 获取歌词失败" << std::endl;
        }
        
        // 下载封面
        if (!data.coverUrl.empty()) {
            if (!downloadImage(data)) {
                std::cout << "QQ音乐: 封面下载失败" << std::endl;
            }
        } else {
            std::cout << "QQ音乐: 无封面URL" << std::endl;
        }
        
        if (data.hasLyrics || data.hasCover) {
            std::cout << "QQ音乐: 获取成功" << std::endl;
            return true;
        }
        
        return false;
    }
    
public:
    MusicMetadataUpdater() {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    
    ~MusicMetadataUpdater() {
        curl_global_cleanup();
    }
    
    bool updateMusicMetadata(const std::string& songName, const std::string& m4aFilePath) {
        std::cout << "========================================" << std::endl;
        std::cout << "开始处理: " << songName << std::endl;
        std::cout << "文件路径: " << m4aFilePath << std::endl;
        
        // 检查文件是否存在
        std::ifstream fileCheck(m4aFilePath);
        if (!fileCheck.good()) {
            std::cerr << "错误: 文件不存在或无法访问: " << m4aFilePath << std::endl;
            return false;
        }
        fileCheck.close();
        
        // 存储各个平台的数据
        std::vector<std::unique_ptr<MusicData>> allData;
        
        // 使用异步同时尝试三个平台
        std::vector<std::future<std::unique_ptr<MusicData>>> futures;
        
        // 网易云
        futures.push_back(std::async(std::launch::async, [this, songName]() {
            auto data = std::make_unique<MusicData>();
            if (this->fetchFromNetease(songName, *data)) {
                return data;
            }
            return std::unique_ptr<MusicData>(nullptr);
        }));
        
        // 酷狗
        futures.push_back(std::async(std::launch::async, [this, songName]() {
            auto data = std::make_unique<MusicData>();
            if (this->fetchFromKugou(songName, *data)) {
                return data;
            }
            return std::unique_ptr<MusicData>(nullptr);
        }));
        
        // QQ音乐
        futures.push_back(std::async(std::launch::async, [this, songName]() {
            auto data = std::make_unique<MusicData>();
            if (this->fetchFromQQMusic(songName, *data)) {
                return data;
            }
            return std::unique_ptr<MusicData>(nullptr);
        }));
        
        // 等待所有结果
        for (auto& future : futures) {
            auto data = future.get();
            if (data) {
                allData.push_back(std::move(data));
            }
        }
        
        // 按照优先级选择数据源: 网易云 > QQ音乐 > 酷狗
        const MusicData* selectedData = nullptr;
        
        for (const auto& data : allData) {
            if (data->source == "网易云音乐" && (data->hasLyrics || data->hasCover)) {
                selectedData = data.get();
                std::cout << "\n选择数据源: 网易云音乐" << std::endl;
                break;
            }
        }
        
        if (!selectedData) {
            for (const auto& data : allData) {
                if (data->source == "QQ音乐" && (data->hasLyrics || data->hasCover)) {
                    selectedData = data.get();
                    std::cout << "\n选择数据源: QQ音乐" << std::endl;
                    break;
                }
            }
        }
        
        if (!selectedData) {
            for (const auto& data : allData) {
                if (data->source == "酷狗音乐" && (data->hasLyrics || data->hasCover)) {
                    selectedData = data.get();
                    std::cout << "\n选择数据源: 酷狗音乐" << std::endl;
                    break;
                }
            }
        }
        
        if (!selectedData) {
            std::cerr << "\n错误: 无法从任何平台获取数据!" << std::endl;
            return false;
        }
        
        // 显示获取到的数据摘要
        std::cout << "\n获取到的数据:" << std::endl;
        std::cout << "歌曲名: " << selectedData->songName << std::endl;
        std::cout << "艺术家: " << selectedData->artist << std::endl;
        if (selectedData->hasAlbum) {
            std::cout << "专辑: " << selectedData->album << std::endl;
        }
        std::cout << "歌词: " << (selectedData->hasLyrics ? "有" : "无") << std::endl;
        std::cout << "封面: " << (selectedData->hasCover ? "有" : "无") << std::endl;
        
        // 写入文件
        return writeToM4AFile(m4aFilePath, *selectedData);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "========================================" << std::endl;
        std::cout << "音乐文件元数据更新工具 v2.1" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "功能: 自动从多个音乐平台获取歌词和封面" << std::endl;
        std::cout << "支持平台: 网易云音乐、酷狗音乐、QQ音乐" << std::endl;
        std::cout << "优先级: 网易云 > QQ音乐 > 酷狗" << std::endl;
        std::cout << "\n使用方法: " << argv[0] << " <歌曲名> <M4A文件路径>" << std::endl;
        std::cout << "\n示例: " << std::endl;
        std::cout << "  " << argv[0] << " \"周杰伦 七里香\" \"song.m4a\"" << std::endl;
        std::cout << "  " << argv[0] << " \"爱错\" \"path/to/song.m4a\"" << std::endl;
        std::cout << "\n注意事项:" << std::endl;
        std::cout << "  1. 歌曲名可以是纯歌曲名或'歌手 歌曲名'格式" << std::endl;
        std::cout << "  2. 自动转换为同步歌词格式（如可用）" << std::endl;
        std::cout << "  3. 支持高质量封面下载" << std::endl;
        std::cout << "  4. 需要网络连接" << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
    
    std::string songName = argv[1];
    std::string filePath = argv[2];
    
    MusicMetadataUpdater updater;
    
    try {
        bool success = updater.updateMusicMetadata(songName, filePath);
        if (success) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "元数据更新完成!" << std::endl;
            std::cout << "推荐播放器:" << std::endl;
            std::cout << "  - Apple Music/iTunes (支持同步歌词)" << std::endl;
            std::cout << "  - VLC Media Player" << std::endl;
            std::cout << "  - Windows Media Player" << std::endl;
            std::cout << "========================================" << std::endl;
            return 0;
        } else {
            std::cerr << "\n更新失败!" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "\n程序异常: " << e.what() << std::endl;
        return 1;
    }
}