#include "lyrics_encapsulator.h"
#include <zlib.h>

using json = nlohmann::json;

std::string mergeBilingualLyrics(const std::string& original, const std::string& translation);

LyricsEncapsulator::LyricsEncapsulator() {
    curl_global_init(CURL_GLOBAL_ALL);
}

LyricsEncapsulator::~LyricsEncapsulator() {
    curl_global_cleanup();
}

std::string LyricsEncapsulator::encodeUrl(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    
    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.length()));
    std::string result(encoded ? encoded : "");
    curl_free(encoded);
    curl_easy_cleanup(curl);
    
    return result;
}

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

bool LyricsEncapsulator::performHttpRequest(const std::string& url, 
                        std::string& response,
                        const std::vector<std::string>& extraHeaders,
                        const std::string& postData,
                        long timeout) {
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


bool LyricsEncapsulator::searchSongFromNetease(const std::string& keyword, MusicData& data) {
    std::string url = "http://music.163.com/api/cloudsearch/pc?s=" + 
                     encodeUrl(keyword) + "&type=1&offset=0&limit=10";
    
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
            
            auto& songs = j["result"]["songs"];
            int bestIndex = 0;
            
            // 选择最佳匹配：歌名相似度优先，播放量做 tiebreak
            if (songs.size() > 1) {
                // 先找最高播放量，用于归一化
                long long maxPop = 0;
                for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                    long long pop = 0;
                    if (songs[i].contains("pop") && songs[i]["pop"].is_number()) {
                        pop = songs[i]["pop"].get<long long>();
                    }
                    if (pop > maxPop) maxPop = pop;
                }
                
                double bestScore = -1.0;
                for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                    std::string songName;
                    if (songs[i].contains("name") && songs[i]["name"].is_string()) {
                        songName = songs[i]["name"].get<std::string>();
                    }
                    double sim = nameSimilarity(keyword, songName);
                    
                    // 歌手名相似度加分（最多+0.2）：关键词中可能包含歌手信息
                    double artistBonus = 0.0;
                    if (songs[i].contains("ar") && !songs[i]["ar"].empty() && songs[i]["ar"][0].contains("name")) {
                        std::string artist = songs[i]["ar"][0]["name"].get<std::string>();
                        artistBonus = 0.2 * nameSimilarity(keyword, artist);
                    }
                    
                    // 播放量加分（最多+0.15）：播放量越高越可能是原版
                    double popBonus = 0.0;
                    if (maxPop > 0 && songs[i].contains("pop") && songs[i]["pop"].is_number()) {
                        long long pop = songs[i]["pop"].get<long long>();
                        popBonus = 0.15 * static_cast<double>(pop) / maxPop;
                    }
                    
                    double score = sim + artistBonus + popBonus;
                    if (score > bestScore) {
                        bestScore = score;
                        bestIndex = i;
                    }
                }
                if (bestIndex != 0) {
                    std::string matchedName = songs[bestIndex].contains("name") ? songs[bestIndex]["name"].get<std::string>() : "";
                    std::cout << "网易云匹配: 选中第" << (bestIndex + 1) << "个结果(" << matchedName << ", 评分=" << bestScore << ")" << std::endl;
                }
            }
            
            auto& song = songs[bestIndex];
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
            
            // 获取封面URL
            if (song.contains("al") && !song["al"].is_null() && song["al"].contains("picUrl")) {
                std::string picUrl = song["al"]["picUrl"].get<std::string>();
                size_t pos = picUrl.find("?");
                if (pos != std::string::npos) {
                    picUrl = picUrl.substr(0, pos);
                }
                // 使用1000x1000最高质量
                data.coverUrl = picUrl + "?param=1000y1000";
            }
            
            return true;
        }
    } catch (const json::exception& e) {
        std::cerr << "网易云搜索JSON解析错误: " << e.what() << std::endl;
    }
    
    return false;
}

bool LyricsEncapsulator:: searchSongFromKugou(const std::string& keyword, MusicData& data) {
    std::string url = "http://mobilecdn.kugou.com/api/v3/search/song?format=json&keyword=" +
                     encodeUrl(keyword) + "&page=1&pagesize=10&showtype=1";
    
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
            
            auto& songs = j["data"]["info"];
            int bestIndex = 0;
            
            // 选择最佳匹配：歌名相似度优先，播放量做 tiebreak
            if (songs.size() > 1) {
                // 先找最高播放量，用于归一化
                long long maxPop = 0;
                for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                    long long pop = 0;
                    if (songs[i].contains("play_count") && songs[i]["play_count"].is_number()) {
                        pop = songs[i]["play_count"].get<long long>();
                    }
                    if (pop > maxPop) maxPop = pop;
                }
                if (maxPop == 0) {
                    // play_count字段不存在，尝试其他字段
                    for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                        long long pop = 0;
                        if (songs[i].contains("privilege_flg") && songs[i]["privilege_flg"].is_number()) {
                            pop = songs[i]["privilege_flg"].get<long long>();
                        }
                        if (pop > maxPop) maxPop = pop;
                    }
                    if (maxPop > 0) std::cout << "酷狗: 使用privilege_flg作为播放量" << std::endl;
                } else {
                    std::cout << "酷狗: 使用play_count作为播放量(max=" << maxPop << ")" << std::endl;
                }
                
                double bestScore = -1.0;
                for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                    std::string songName;
                    if (songs[i].contains("songname") && songs[i]["songname"].is_string()) {
                        songName = songs[i]["songname"].get<std::string>();
                    }
                    double sim = nameSimilarity(keyword, songName);
                    
                    // 歌手名相似度加分（最多+0.2）
                    double artistBonus = 0.0;
                    if (songs[i].contains("singername") && songs[i]["singername"].is_string()) {
                        std::string artist = songs[i]["singername"].get<std::string>();
                        artistBonus = 0.2 * nameSimilarity(keyword, artist);
                    }
                    
                    double popBonus = 0.0;
                    if (maxPop > 0) {
                        long long pop = 0;
                        if (songs[i].contains("play_count") && songs[i]["play_count"].is_number()) {
                            pop = songs[i]["play_count"].get<long long>();
                        } else if (songs[i].contains("privilege_flg") && songs[i]["privilege_flg"].is_number()) {
                            pop = songs[i]["privilege_flg"].get<long long>();
                        }
                        popBonus = 0.15 * static_cast<double>(pop) / maxPop;
                    }
                    
                    double score = sim + artistBonus + popBonus;
                    if (score > bestScore) {
                        bestScore = score;
                        bestIndex = i;
                    }
                }
                if (bestIndex != 0) {
                    std::string matchedName = songs[bestIndex].contains("songname") ? songs[bestIndex]["songname"].get<std::string>() : "";
                    std::cout << "酷狗匹配: 选中第" << (bestIndex + 1) << "个结果(" << matchedName << ", 评分=" << bestScore << ")" << std::endl;
                }
            }
            
            auto& song = songs[bestIndex];
            
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
            
            if (song.contains("album_img") && !song["album_img"].is_null()) {
                std::string albumImg = song["album_img"].get<std::string>();
                if (!albumImg.empty() && albumImg.find("http") == 0) {
                    albumImg.erase(std::remove(albumImg.begin(), albumImg.end(), ' '), albumImg.end());
                    if (albumImg.find("_150.jpg") != std::string::npos) {
                        data.coverUrl = std::regex_replace(albumImg, std::regex("_150\\.jpg$"), "_500.jpg");
                    } else {
                        data.coverUrl = albumImg;
                    }
                }
            }
            
            if (song.contains("img") && !song["img"].is_null()) {
                std::string imgUrl = song["img"].get<std::string>();
                if (!imgUrl.empty() && imgUrl.find("http") == 0) {
                    imgUrl.erase(std::remove(imgUrl.begin(), imgUrl.end(), ' '), imgUrl.end());
                    data.coverUrl = imgUrl;
                }
            }

            if (data.coverUrl.empty() && song.contains("trans_param") && song["trans_param"].is_object()) {
                auto& tp = song["trans_param"];
                if (tp.contains("union_cover") && tp["union_cover"].is_string()) {
                    std::string unionCover = tp["union_cover"].get<std::string>();
                    if (!unionCover.empty() && unionCover.find("http") == 0) {
                        unionCover.erase(std::remove(unionCover.begin(), unionCover.end(), ' '), unionCover.end());
                        size_t pos = unionCover.find("{size}");
                        if (pos != std::string::npos) {
                            unionCover.replace(pos, 6, "800");
                        }
                        data.coverUrl = unionCover;
                    }
                }
            }

            if (data.coverUrl.empty() && song.contains("album_id")) {
                std::string albumId;
                if (song["album_id"].is_string()) {
                    albumId = song["album_id"].get<std::string>();
                } else if (song["album_id"].is_number()) {
                    albumId = std::to_string(song["album_id"].get<long long>());
                }
                if (!albumId.empty()) {
                    std::string coverUrl = fetchKugouAlbumCover(albumId);
                    if (!coverUrl.empty()) {
                        data.coverUrl = coverUrl;
                    }
                }
            }
            
            // 保存歌曲时长
            
            return true;
        }
    } catch (const json::exception& e) {
        std::cerr << "酷狗搜索JSON解析错误: " << e.what() << std::endl;
    }
    
    return false;
}

std::string LyricsEncapsulator::fetchKugouAlbumCover(const std::string& album_id) {
    if (album_id.empty()) return "";

    std::string url = "http://mobilecdn.kugou.com/api/v3/album/info?albumid=" + album_id;
    std::string response;

    if (!performHttpRequest(url, response)) {
        std::cerr << "酷狗专辑信息请求失败: album_id=" << album_id << std::endl;
        return "";
    }

    try {
        json j = json::parse(response);
        if (j.contains("status") && j["status"].get<int>() == 1 && j.contains("data")) {
            auto& albumData = j["data"];

            if (albumData.contains("imgurl") && !albumData["imgurl"].is_null()) {
                std::string coverUrl = albumData["imgurl"].get<std::string>();
                if (!coverUrl.empty() && coverUrl.find("http") == 0) {
                    coverUrl.erase(std::remove(coverUrl.begin(), coverUrl.end(), ' '), coverUrl.end());
                    size_t pos = coverUrl.find("{size}");
                    if (pos != std::string::npos) {
                        coverUrl.replace(pos, 6, "800");
                    }
                    std::cout << "酷狗专辑封面: " << coverUrl << std::endl;
                    return coverUrl;
                }
            }

            if (albumData.contains("sizable_cover") && !albumData["sizable_cover"].is_null()) {
                std::string coverUrl = albumData["sizable_cover"].get<std::string>();
                if (!coverUrl.empty()) {
                    coverUrl.erase(std::remove(coverUrl.begin(), coverUrl.end(), ' '), coverUrl.end());
                    size_t pos = coverUrl.find("{size}");
                    if (pos != std::string::npos) {
                        coverUrl.replace(pos, 6, "800");
                    }
                    std::cout << "酷狗专辑封面(高质量): " << coverUrl << std::endl;
                    return coverUrl;
                }
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "酷狗专辑信息JSON解析错误: " << e.what() << std::endl;
    }

    return "";
}

std::string LyricsEncapsulator::adjustLyricsTiming(const std::string& lyrics, int offsetMs) {
    // 匹配 [mm:ss.xx] 或 [mm:ss.xxx] 格式的时间戳
    std::regex timeRegex(R"(\[(\d{2}):(\d{2})(?:\.(\d{2,3}))?\])");
    std::string result;
    std::string::const_iterator searchStart(lyrics.cbegin());
    std::smatch match;
    
    while (std::regex_search(searchStart, lyrics.cend(), match, timeRegex)) {
        // 添加匹配前的文本（保留非时间戳标签如[ti:]、[ar:]等元数据）
        result += match.prefix().str();
        
        // 解析时间
        int minutes = std::stoi(match[1].str());
        int seconds = std::stoi(match[2].str());
        int milliseconds = 0;
        
        if (match[3].matched) {
            std::string msStr = match[3].str();
            if (msStr.length() == 2) {
                milliseconds = std::stoi(msStr) * 10;  // 百分之一秒转毫秒
            } else if (msStr.length() == 3) {
                milliseconds = std::stoi(msStr);       // 毫秒
            }
        }
        
        // 计算总毫秒数并添加偏移（正值为延迟/推后，负值为提前）
        int totalMs = minutes * 60000 + seconds * 1000 + milliseconds;
        totalMs += offsetMs;
        
        if (totalMs < 0) totalMs = 0;
        
        // 转换回 [mm:ss.xx] 格式（保留百分之一秒精度，与酷狗原格式一致）
        int newMin = totalMs / 60000;
        int newSec = (totalMs % 60000) / 1000;
        int newMs = (totalMs % 1000) / 10;  // 转换为百分之一秒
        
        std::ostringstream oss;
        oss << "[" << std::setw(2) << std::setfill('0') << newMin << ":"
            << std::setw(2) << std::setfill('0') << newSec << "."
            << std::setw(2) << std::setfill('0') << newMs << "]";
        result += oss.str();
        
        searchStart = match.suffix().first;
    }
    
    // 添加剩余文本（歌词内容）
    result += std::string(searchStart, lyrics.cend());
    return result;
}

std::string LyricsEncapsulator::generateRandomMid() {
    const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::string mid;
    for (int i = 0; i < 32; ++i) {
        mid += hex[dis(gen)];
    }
    return mid;
}

bool LyricsEncapsulator::searchSongFromQQMusic(const std::string& keyword, MusicData& data) {
    json reqJson;
    reqJson["comm"] = {{"ct", 19}, {"cv", "1859"}, {"uin", "0"}};
    reqJson["req"]["method"] = "DoSearchForQQMusicDesktop";
    reqJson["req"]["module"] = "music.search.SearchCgiService";
    reqJson["req"]["param"]["query"] = keyword;
    reqJson["req"]["param"]["num_per_page"] = 10;
    reqJson["req"]["param"]["page_num"] = 1;

    std::string jsonData = reqJson.dump();
    std::string url = "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json";

    std::string response;
    std::vector<std::string> headers = {
        "Referer: https://y.qq.com",
        "Origin: https://y.qq.com",
        "Content-Type: application/json"
    };

    if (!performHttpRequest(url, response, headers, jsonData)) {
        std::cerr << "QQ音乐搜索: POST请求失败，尝试GET方式..." << std::endl;
        std::string getUrl = "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json&data=" + encodeUrl(jsonData);
        std::vector<std::string> getHeaders = {
            "Referer: https://y.qq.com",
            "Origin: https://y.qq.com"
        };
        if (!performHttpRequest(getUrl, response, getHeaders)) {
            return false;
        }
    }

    if (response.empty()) {
        std::cerr << "QQ音乐搜索: 响应为空" << std::endl;
        return false;
    }

    try {
        json j = json::parse(response);
        json* songList = nullptr;

        if (j.contains("req") && j["req"].contains("data") &&
            j["req"]["data"].contains("body") &&
            j["req"]["data"]["body"].contains("song") &&
            j["req"]["data"]["body"]["song"].contains("list")) {
            songList = &j["req"]["data"]["body"]["song"]["list"];
        }

        if (!songList || songList->empty()) {
            std::cerr << "QQ音乐搜索: 未找到歌曲" << std::endl;
            return false;
        }

        auto& songs = *songList;
        int bestIndex = 0;
        
        // 选择最佳匹配：歌名相似度优先，播放量+时长做 tiebreak
        if (songs.size() > 1) {
            // 先找最高播放量，用于归一化
            long long maxPop = 0;
            for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                long long pop = 0;
                if (songs[i].contains("listenCnt") && songs[i]["listenCnt"].is_number()) {
                    pop = songs[i]["listenCnt"].get<long long>();
                }
                if (pop > maxPop) maxPop = pop;
            }
            
            double bestScore = -1.0;
            for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                std::string songName;
                if (songs[i].contains("name") && songs[i]["name"].is_string()) {
                    songName = songs[i]["name"].get<std::string>();
                }
                double sim = nameSimilarity(keyword, songName);
                
                // 歌手名相似度加分（最多+0.2）
                double artistBonus = 0.0;
                if (songs[i].contains("singer") && songs[i]["singer"].is_array() && !songs[i]["singer"].empty() && songs[i]["singer"][0].contains("name")) {
                    std::string artist = songs[i]["singer"][0]["name"].get<std::string>();
                    artistBonus = 0.2 * nameSimilarity(keyword, artist);
                }
                
                double popBonus = 0.0;
                if (maxPop > 0 && songs[i].contains("listenCnt") && songs[i]["listenCnt"].is_number()) {
                    long long pop = songs[i]["listenCnt"].get<long long>();
                    popBonus = 0.15 * static_cast<double>(pop) / maxPop;
                }
                
                double score = sim + artistBonus + popBonus;
                if (score > bestScore) {
                    bestScore = score;
                    bestIndex = i;
                }
            }
            if (bestIndex != 0) {
                std::string matchedName = songs[bestIndex].contains("name") ? songs[bestIndex]["name"].get<std::string>() : "";
                std::cout << "QQ音乐匹配: 选中第" << (bestIndex + 1) << "个结果(" << matchedName << ", 评分=" << bestScore << ")" << std::endl;
            }
        }

        auto& song = songs[bestIndex];

        if (song.contains("mid") && song["mid"].is_string()) {
            data.songId = song["mid"].get<std::string>();
        }

        if (song.contains("name") && song["name"].is_string()) {
            data.songName = song["name"].get<std::string>();
        }

        if (song.contains("singer") && song["singer"].is_array() && !song["singer"].empty()) {
            if (song["singer"][0].contains("name") && song["singer"][0]["name"].is_string()) {
                data.artist = song["singer"][0]["name"].get<std::string>();
            }
        }

        if (song.contains("album") && song["album"].is_object()) {
            if (song["album"].contains("name") && song["album"]["name"].is_string()) {
                data.album = song["album"]["name"].get<std::string>();
                data.hasAlbum = true;
            }

            if (song["album"].contains("mid") && song["album"]["mid"].is_string()) {
                std::string albumMid = song["album"]["mid"].get<std::string>();
                if (!albumMid.empty()) {
                    data.coverUrl = "https://y.gtimg.cn/music/photo_new/T002R800x800M000" +
                                   albumMid + ".jpg";
                }
            }
        }

        return true;
    } catch (const json::exception& e) {
        std::cerr << "QQ音乐搜索JSON解析错误: " << e.what() << std::endl;
    }

    return false;
}

std::string LyricsEncapsulator::base64Decode(const std::string& encoded) {
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

bool LyricsEncapsulator::decryptKRC(const std::vector<uint8_t>& krcData, std::string& outLyrics, std::string& outTranslation) {
    if (krcData.size() < 8) return false;
    if (krcData[0] != 'k' || krcData[1] != 'r' || krcData[2] != 'c' || krcData[3] != '1') {
        std::cerr << "KRC文件头校验失败" << std::endl;
        return false;
    }

    static const uint8_t xorKey[] = {64, 71, 97, 119, 94, 50, 116, 71, 81, 54, 49, 45, 206, 210, 110, 105};
    std::vector<uint8_t> decrypted(krcData.size() - 4);
    for (size_t i = 4; i < krcData.size(); i++) {
        decrypted[i - 4] = krcData[i] ^ xorKey[(i - 4) % 16];
    }

    uLongf destLen = decrypted.size() * 10;
    std::vector<uint8_t> decompressed(destLen);
    int ret = uncompress(decompressed.data(), &destLen, decrypted.data(), decrypted.size());
    if (ret != Z_OK) {
        std::cerr << "KRC zlib解压失败: " << ret << std::endl;
        return false;
    }
    decompressed.resize(destLen);

    std::string krcText(decompressed.begin(), decompressed.end());
    outLyrics = krcText;
    outTranslation.clear();
    return true;
}

std::string LyricsEncapsulator::extractTranslationFromKRCMetadata(const std::string& krcText, const std::string& /*originalLyrics*/) {
    std::regex langRegex(R"(\[language:([^\]]+)\])");
    std::smatch match;
    if (!std::regex_search(krcText, match, langRegex)) {
        return "";
    }

    std::string b64Data = match[1].str();
    std::string jsonData = base64Decode(b64Data);
    if (jsonData.empty()) return "";

    try {
        json j = json::parse(jsonData);
        if (!j.contains("content") || !j["content"].is_array() || j["content"].empty()) return "";
        auto& firstLang = j["content"][0];
        if (!firstLang.contains("lyricContent") || !firstLang["lyricContent"].is_array()) return "";

        auto& lyricContent = firstLang["lyricContent"];
        std::vector<std::string> translations;
        for (auto& line : lyricContent) {
            if (line.is_array() && !line.empty()) {
                std::string transLine = line[0].get<std::string>();
                if (!transLine.empty()) {
                    size_t end = transLine.find_last_not_of(" \t\r\n");
                    if (end != std::string::npos) transLine = transLine.substr(0, end + 1);
                    translations.push_back(transLine);
                } else {
                    translations.push_back("");
                }
            }
        }

        std::regex krcLineRegex(R"(\[(\d+),\d+\])");
        std::vector<int> krcStartTimes;
        std::istringstream krcIss(krcText);
        std::string krcLine;
        while (std::getline(krcIss, krcLine)) {
            std::smatch m;
            if (std::regex_search(krcLine, m, krcLineRegex)) {
                krcStartTimes.push_back(std::stoi(m[1].str()));
            }
        }

        std::ostringstream result;
        size_t transIdx = 0;
        for (int startTime : krcStartTimes) {
            if (transIdx < translations.size() && !translations[transIdx].empty()) {
                int mm = startTime / 60000;
                int ss = (startTime % 60000) / 1000;
                int xx = (startTime % 1000) / 10;
                char tsStr[32];
                std::snprintf(tsStr, sizeof(tsStr), "[%02d:%02d.%02d]", mm, ss, xx);
                result << tsStr << translations[transIdx] << "\n";
            }
            transIdx++;
        }

        return result.str();
    } catch (const json::exception& e) {
        std::cerr << "KRC翻译JSON解析错误: " << e.what() << std::endl;
        return "";
    }
}

// 计算两个歌名的相似度（0~1，1=完全匹配）
// 基于关键词重叠率：将歌名拆分为小写词，计算交集占比
double LyricsEncapsulator::nameSimilarity(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return 0.0;
    
    // 提取关键词：转小写，按空格/标点拆分
    auto extractWords = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> words;
        std::string word;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                word += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            } else if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        if (!word.empty()) words.push_back(word);
        return words;
    };
    
    auto wordsA = extractWords(a);
    auto wordsB = extractWords(b);
    if (wordsA.empty() || wordsB.empty()) return 0.0;
    
    // 计算交集大小
    std::sort(wordsA.begin(), wordsA.end());
    std::sort(wordsB.begin(), wordsB.end());
    std::vector<std::string> intersection;
    std::set_intersection(wordsA.begin(), wordsA.end(), wordsB.begin(), wordsB.end(),
                          std::back_inserter(intersection));
    
    // 用较短词集的大小做分母，避免短歌名天然高分
    size_t minSize = std::min(wordsA.size(), wordsB.size());
    return static_cast<double>(intersection.size()) / minSize;
}

bool LyricsEncapsulator::isValidImage(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return false;
    
    // JPEG
    if (data[0] == 0xFF && data[1] == 0xD8) return true;
    
    // PNG
    if (data[0] == 0x89 && data[1] == 0x50 && 
        data[2] == 0x4E && data[3] == 0x47) return true;
    
    // WebP
    if (data[0] == 0x52 && data[1] == 0x49 && 
        data[2] == 0x46 && data[3] == 0x46 &&
        data.size() > 12 &&
        data[8] == 0x57 && data[9] == 0x45 &&
        data[10] == 0x42 && data[11] == 0x50) return true;
    
    return false;
}

std::vector<uint8_t> LyricsEncapsulator::downloadImageFromUrl(const std::string& url) {
    if (url.empty()) {
        return {};
    }
    
    std::string cleanUrl = std::regex_replace(url, std::regex("\\s+"), "");
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {};
    }
    
    std::vector<uint8_t> coverData;
    long responseCode = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, cleanUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBinaryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &coverData);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: image/webp,image/apng,image/*,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        if (responseCode == 200 && !coverData.empty()) {
            if (isValidImage(coverData)) {
                if (coverData.size() < 5120) {
                    std::cerr << "封面过小 (" << coverData.size()
                              << " 字节)，可能是空白/占位封面，已跳过" << std::endl;
                } else {
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    return coverData;
                }
            }
        }
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return {};
}


bool LyricsEncapsulator::setMP4CoverArt(TagLib::MP4::File& file, const std::vector<uint8_t>& coverData) {
    try {
        TagLib::MP4::CoverArt::Format format = TagLib::MP4::CoverArt::JPEG;

        if (coverData.size() > 8) {
            if (coverData[0] == 0x89 && coverData[1] == 0x50 && 
                coverData[2] == 0x4E && coverData[3] == 0x47) {
                format = TagLib::MP4::CoverArt::PNG;
            }
            else if (coverData[0] == 0xFF && coverData[1] == 0xD8) {
                format = TagLib::MP4::CoverArt::JPEG;
            }
        }

        TagLib::ByteVector coverVector(
            reinterpret_cast<const char*>(coverData.data()), 
            coverData.size()
        );

        TagLib::MP4::CoverArt coverArt(format, coverVector);
        TagLib::MP4::CoverArtList coverList;
        coverList.append(coverArt);

        TagLib::MP4::Tag* tag = file.tag();
        if (tag) {
            tag->setItem("covr", TagLib::MP4::Item(coverList));
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "设置封面失败: " << e.what() << std::endl;
    }

    return false;
}


std::string LyricsEncapsulator::convertLRCToStandardFormat(const std::string& lrcText) {
    std::istringstream stream(lrcText);
    std::string line;
    std::vector<std::pair<int, std::string>> lyrics;
    
    std::regex timeRegex(R"(\[(\d+):(\d+)(?:\.(\d+))?\])");
    
    while (std::getline(stream, line)) {
        std::smatch match;
        std::string::const_iterator searchStart(line.cbegin());
        
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
        
        std::string lyricText = std::regex_replace(line, timeRegex, "");
        lyricText = std::regex_replace(lyricText, std::regex("^\\s+|\\s+$"), "");
        
        if (!lyricText.empty() && !times.empty()) {
            for (int time : times) {
                lyrics.push_back(std::make_pair(time, lyricText));
            }
        }
    }
    
    std::sort(lyrics.begin(), lyrics.end(),
             [](const std::pair<int, std::string>& a, 
                const std::pair<int, std::string>& b) { 
                 return a.first < b.first; 
             });
    
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

std::string LyricsEncapsulator::cleanLyrics(const std::string& lyrics) {
    if (lyrics.empty()) return "";
    
    std::string cleaned = lyrics;
    
    if (cleaned.size() >= 3 && 
        static_cast<unsigned char>(cleaned[0]) == 0xEF &&
        static_cast<unsigned char>(cleaned[1]) == 0xBB &&
        static_cast<unsigned char>(cleaned[2]) == 0xBF) {
        cleaned = cleaned.substr(3);
    }
    
    if (cleaned.find('[') != std::string::npos && 
        cleaned.find(']') != std::string::npos &&
        cleaned.find(':') != std::string::npos) {
        return convertLRCToStandardFormat(cleaned);
    } else {
        cleaned = std::regex_replace(cleaned, std::regex("<[^>]*>"), "");
        cleaned = std::regex_replace(cleaned, std::regex("\\s+"), " ");
        cleaned = std::regex_replace(cleaned, std::regex("\\n\\s*\\n"), "\n");
        cleaned = std::regex_replace(cleaned, std::regex("^\\s+|\\s+$"), "");
        return cleaned;
    }
}

bool LyricsEncapsulator::setMP4Lyrics(TagLib::MP4::File& file, const std::string& lyrics) {
    try {
        std::string cleanedLyrics = cleanLyrics(lyrics);
        TagLib::String lyricsStr(cleanedLyrics, TagLib::String::UTF8);

        TagLib::MP4::Tag* tag = file.tag();
        if (tag) {
            tag->setItem("\xA9lyr", TagLib::MP4::Item(lyricsStr));
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "设置歌词失败: " << e.what() << std::endl;
    }

    return false;
}

bool LyricsEncapsulator::writeToM4AFile(const std::string& filepath, const MusicData& data) {
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

    // 设置基本标签
    if (!data.songName.empty()) {
        tag->setTitle(TagLib::String(data.songName, TagLib::String::UTF8));
        std::cout << "标题: " << data.songName << std::endl;
    }

    if (!data.artist.empty()) {
        tag->setArtist(TagLib::String(data.artist, TagLib::String::UTF8));
        std::cout << "艺术家: " << data.artist << std::endl;
    }

    if (!data.album.empty()) {
        tag->setAlbum(TagLib::String(data.album, TagLib::String::UTF8));
        std::cout << "专辑: " << data.album << std::endl;
    }

    // 写入封面
    bool coverWritten = false;
    if (data.hasCover && !data.coverData.empty()) {
        coverWritten = setMP4CoverArt(file, data.coverData);
        if (coverWritten) {
            std::cout << "封面: 写入成功 (" << data.coverData.size() << " 字节)" << std::endl;
        } else {
            std::cerr << "封面: 写入失败" << std::endl;
        }
    } else {
        std::cout << "封面: 无封面数据" << std::endl;
    }

    // 写入歌词
    bool lyricsWritten = false;
    if (data.hasLyrics && !data.lyrics.empty()) {
        lyricsWritten = setMP4Lyrics(file, data.lyrics);
        if (lyricsWritten) {
            std::cout << "歌词: 写入成功" << std::endl;
        } else {
            std::cerr << "歌词: 写入失败" << std::endl;
        }
    } else {
        std::cout << "歌词: 无歌词数据" << std::endl;
    }

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

// 选择最佳的歌词（优先酷狗）
std::pair<std::string, bool> LyricsEncapsulator::getBestLyrics(const std::vector<std::unique_ptr<MusicData>>& allData, const std::string platform) {
    std::vector<std::string> priority = {"酷狗音乐", "网易云音乐", "QQ音乐"};
    if (!platform.empty()) priority[0] = platform;

    const MusicData* selectedData = nullptr;

    for (const auto& source : priority) {
        for (const auto& data : allData) {
            if (data->source == source && data->hasLyrics && !data->lyrics.empty()) {
                selectedData = data.get();
                break;
            }
        }
        if (selectedData) break;
    }

    if (!selectedData) {
        for (const auto& data : allData) {
            if (data->hasLyrics && !data->lyrics.empty()) {
                selectedData = data.get();
                break;
            }
        }
    }

    if (!selectedData) {
        std::cout << "未找到歌词" << std::endl;
        return {"", false};
    }

    std::string lyrics = selectedData->lyrics;
    std::cout << "使用" << selectedData->source << "的歌词" << std::endl;

    if (!selectedData->hasTranslation) {
        std::vector<std::string> transPriority = {"网易云音乐", "QQ音乐", "酷狗音乐"};
        for (const auto& source : transPriority) {
            if (source == selectedData->source) continue;
            for (const auto& data : allData) {
                if (data->source == source && data->hasTranslation && !data->translationLyrics.empty()) {
                    std::cout << "从" << source << "补充翻译歌词，合并双语..." << std::endl;
                    lyrics = mergeBilingualLyrics(lyrics, data->translationLyrics);
                    break;
                }
            }
            if (lyrics != selectedData->lyrics) break;
        }
    } else {
        lyrics = mergeBilingualLyrics(lyrics, selectedData->translationLyrics);
    }

    return {lyrics, true};
}

bool LyricsEncapsulator::updateMusicMetadata(const std::string& songName, const std::string& m4aFilePath,
                                             const std::string& platform, const int offsetMs) {
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
    
    // 使用异步同时从三个平台获取数据
    std::vector<std::future<std::unique_ptr<MusicData>>> futures;
    
    // 网易云
    futures.push_back(std::async(std::launch::async, [this, songName]() {
        auto data = std::make_unique<MusicData>();
        if (this->searchSongFromNetease(songName, *data)) {
            data->source = "网易云音乐";
            // 获取歌词
            if (!this->getLyricsFromNetease(*data)) {
                data->hasLyrics = false;
            }
            return data;
        }
        return std::unique_ptr<MusicData>(nullptr);
    }));
    
    // 酷狗
    futures.push_back(std::async(std::launch::async, [this, songName]() {
        auto data = std::make_unique<MusicData>();
        if (this->searchSongFromKugou(songName, *data)) {
            data->source = "酷狗音乐";
            // 获取歌词（优先）
            if (!this->getLyricsFromKugou(*data)) {
                data->hasLyrics = false;
            }
            return data;
        }
        return std::unique_ptr<MusicData>(nullptr);
    }));
    
    // QQ音乐
    futures.push_back(std::async(std::launch::async, [this, songName]() {
        auto data = std::make_unique<MusicData>();
        if (this->searchSongFromQQMusic(songName, *data)) {
            data->source = "QQ音乐";
            // 获取歌词
            if (!this->getLyricsFromQQMusic(*data)) {
                data->hasLyrics = false;
            }
            return data;
        }
        return std::unique_ptr<MusicData>(nullptr);
    }));
    
    // 等待所有数据获取完成
    std::vector<std::unique_ptr<MusicData>> allData;
    for (auto& future : futures) {
        auto data = future.get();
        if (data) {
            allData.push_back(std::move(data));
        }
    }
    
    if (allData.empty()) {
        std::cerr << "\n错误: 无法从任何平台获取数据!" << std::endl;
        return false;
    }
    
    // 选择最佳的数据源（用于基本信息）
    const MusicData* selectedData = nullptr;
    std::vector<std::string> priority = {"酷狗音乐", "网易云音乐", "QQ音乐"};
    // 如果指定了平台，则用指定平台
    if (!platform.empty()) priority[0] = platform;

    for (const auto& source : priority) {
        for (const auto& data : allData) {
            if (data->source == source) {
                selectedData = data.get();
                std::cout << "\n选择基本信息源: " << source << std::endl;
                break;
            }
        }
        if (selectedData) break;
    }
    
    if (!selectedData) {
        selectedData = allData[0].get();
    }
    
    // 获取最佳封面（从所有平台下载并选择最大的）
    auto bestCover = getBestCoverFromAllPlatforms(allData, platform);
    
    // 获取最佳歌词（优先酷狗）
    auto lyricsResult = getBestLyrics(allData, platform);
    std::string bestLyrics = lyricsResult.first;
    bool hasLyrics = lyricsResult.second;
    
    // 歌词偏移处理（仅手动偏移）
    if (hasLyrics && !bestLyrics.empty() && offsetMs != 0) {
        std::cout << "应用歌词偏移: " << offsetMs << "ms" << std::endl;
        bestLyrics = adjustLyricsTiming(bestLyrics, offsetMs);
    }
    
    // 组合最终数据
    MusicData finalData;
    finalData.source = selectedData->source;
    finalData.songName = selectedData->songName;
    finalData.artist = selectedData->artist;
    finalData.album = selectedData->album;
    finalData.hasAlbum = selectedData->hasAlbum;
    finalData.lyrics = bestLyrics;
    finalData.hasLyrics = hasLyrics;
    
    if (!bestCover.empty()) {
        finalData.coverData = std::move(bestCover);
        finalData.hasCover = true;
        finalData.coverSize = finalData.coverData.size();
    }
    
    // 显示最终数据摘要
    std::cout << "\n最终数据:" << std::endl;
    std::cout << "歌曲名: " << finalData.songName << std::endl;
    std::cout << "艺术家: " << finalData.artist << std::endl;
    if (finalData.hasAlbum) {
        std::cout << "专辑: " << finalData.album << std::endl;
    }
    std::cout << "歌词: " << (finalData.hasLyrics ? "有" : "无") << std::endl;
    std::cout << "封面: " << (finalData.hasCover ? "有 (" + std::to_string(finalData.coverSize) + " 字节)" : "无") << std::endl;
    
    // 写入文件
    return writeToM4AFile(m4aFilePath, finalData);
}

/* 歌词获取函数 */

// 辅助函数：合并双语歌词，椒盐音乐格式（双时间戳）
std::string mergeBilingualLyrics(const std::string& original, const std::string& translation) {
    if (translation.empty()) return original;
    
    // key: 时间戳(毫秒)，value: pair<原文, 译文>
    std::map<int, std::pair<std::string, std::string>> merged;
    std::vector<std::string> metaData;
    
    // 正则匹配 LRC 时间戳行 [mm:ss.xx] 或 [mm:ss.xxx]
    std::regex lrcLineRegex(R"(^\[(\d{2}):(\d{2})\.(\d{2,3})\](.*)$)");
    std::smatch match;
    
    // 解析原文和译文的逻辑保持不变...
    std::istringstream origStream(original);
    std::string line;
    while (std::getline(origStream, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        
        if (std::regex_match(line, match, lrcLineRegex)) {
            int mm = std::stoi(match[1].str());
            int ss = std::stoi(match[2].str());
            std::string msStr = match[3].str();
            int ms = std::stoi(msStr);
            if (msStr.length() == 2) ms *= 10;
            int timestamp = mm * 60000 + ss * 1000 + ms;
            
            std::string content = match[4].str();
            size_t start = content.find_first_not_of(" \t");
            if (start != std::string::npos) {
                size_t end = content.find_last_not_of(" \t");
                content = content.substr(start, end - start + 1);
            } else {
                content = "";
            }
            
            merged[timestamp].first = content;
        } else if (!line.empty() && line[0] == '[') {
            metaData.push_back(line);
        }
    }
    
    // 解析译文
    std::istringstream transStream(translation);
    while (std::getline(transStream, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        
        if (std::regex_match(line, match, lrcLineRegex)) {
            int mm = std::stoi(match[1].str());
            int ss = std::stoi(match[2].str());
            std::string msStr = match[3].str();
            int ms = std::stoi(msStr);
            if (msStr.length() == 2) ms *= 10;
            int timestamp = mm * 60000 + ss * 1000 + ms;
            
            std::string content = match[4].str();
            size_t start = content.find_first_not_of(" \t");
            if (start != std::string::npos) {
                size_t end = content.find_last_not_of(" \t");
                content = content.substr(start, end - start + 1);
            } else {
                content = "";
            }
            
            merged[timestamp].second = content;
        }
    }
    
    // 构建结果 - 双时间戳格式（椒盐音乐标准格式）
    std::stringstream result;
    
    // 先输出元数据
    for (const auto& meta : metaData) {
        result << meta << "\n";
    }
    
    // 按时间戳排序输出
    for (const auto& pair : merged) {
        int ts = pair.first;
        int mm = ts / 60000;
        int ss = (ts % 60000) / 1000;
        int xx = (ts % 1000) / 10;
        
        char tsStr[32];
        std::snprintf(tsStr, sizeof(tsStr), "[%02d:%02d.%02d]", mm, ss, xx);
        
        // 输出原文（带时间戳）
        if (!pair.second.first.empty()) {
            result << tsStr << pair.second.first << "\n";
        }
        
        // 输出译文（也带相同时间戳 - 这是关键！）
        if (!pair.second.second.empty() && pair.second.second != pair.second.first) {
            result << tsStr << pair.second.second << "\n";  // 关键修改：加上时间戳
        }
    }
    
    return result.str();
}

// 网易云获取歌词
bool LyricsEncapsulator::getLyricsFromNetease(MusicData& data) {
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
            std::string originalLyrics = j["lrc"]["lyric"].get<std::string>();

            data.lyrics = originalLyrics;

            if (j.contains("tlyric") && j["tlyric"].contains("lyric")) {
                auto& tlyricJson = j["tlyric"]["lyric"];
                if (!tlyricJson.is_null()) {
                    std::string transLyric = tlyricJson.get<std::string>();
                    if (!transLyric.empty()) {
                        std::cout << "获取到网易云翻译歌词" << std::endl;
                        data.translationLyrics = transLyric;
                        data.hasTranslation = true;
                    }
                }
            }

            data.hasLyrics = true;
            return true;
        }
    } catch (const json::exception& e) {
        std::cerr << "网易云歌词JSON解析错误: " << e.what() << std::endl;
    }

    return false;
}

// QQ音乐获取歌词
bool LyricsEncapsulator::getLyricsFromQQMusic(MusicData& data) {
    json reqJson;
    reqJson["comm"] = {{"ct", 19}, {"cv", "1859"}, {"uin", "0"}};
    reqJson["req"]["method"] = "GetPlayLyricInfo";
    reqJson["req"]["module"] = "music.musichallSong.PlayLyricInfo";
    reqJson["req"]["param"]["songMID"] = data.songId;

    std::string jsonData = reqJson.dump();
    std::string url = "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json";

    std::string response;
    std::vector<std::string> headers = {
        "Referer: https://y.qq.com",
        "Origin: https://y.qq.com",
        "Content-Type: application/json"
    };

    if (!performHttpRequest(url, response, headers, jsonData)) {
        std::cerr << "QQ音乐歌词: POST请求失败，尝试GET方式..." << std::endl;
        std::string getUrl = "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json&data=" + encodeUrl(jsonData);
        std::vector<std::string> getHeaders = {
            "Referer: https://y.qq.com",
            "Origin: https://y.qq.com"
        };
        if (!performHttpRequest(getUrl, response, getHeaders)) {
            return false;
        }
    }

    if (response.empty()) {
        std::cerr << "QQ音乐歌词: 响应为空" << std::endl;
        return false;
    }

    try {
        json j = json::parse(response);

        if (!j.contains("req") || !j["req"].contains("data")) {
            std::cerr << "QQ音乐歌词: 响应格式异常" << std::endl;
            return false;
        }

        auto& lyricData = j["req"]["data"];

        std::string originalLyrics;
        if (lyricData.contains("lyric") && lyricData["lyric"].is_string()) {
            std::string base64Lyric = lyricData["lyric"].get<std::string>();
            if (!base64Lyric.empty()) {
                originalLyrics = base64Decode(base64Lyric);
            }
        }

        if (originalLyrics.empty()) {
            std::cerr << "QQ音乐歌词: 歌词内容为空" << std::endl;
            return false;
        }

        if (lyricData.contains("trans") && lyricData["trans"].is_string()) {
            std::string base64Trans = lyricData["trans"].get<std::string>();
            if (!base64Trans.empty()) {
                std::string transLyric = base64Decode(base64Trans);
                if (!transLyric.empty()) {
                    std::cout << "获取到QQ音乐翻译歌词" << std::endl;
                    data.translationLyrics = transLyric;
                    data.hasTranslation = true;
                }
            }
        }

        data.lyrics = originalLyrics;

        data.hasLyrics = !data.lyrics.empty();
        return data.hasLyrics;
    } catch (const json::exception& e) {
        std::cerr << "QQ音乐歌词JSON解析错误: " << e.what() << std::endl;
    }

    return false;
}

// 从酷狗获取歌词
bool LyricsEncapsulator::getLyricsFromKugou(MusicData& data) {
    if (data.songId.empty() && data.songName.empty()) {
        return false;
    }

    std::string searchKeyword;
    if (!data.artist.empty() && !data.songName.empty()) {
        searchKeyword = data.artist + " - " + data.songName;
    } else if (!data.songName.empty()) {
        searchKeyword = data.songName;
    } else {
        searchKeyword = data.songId;
    }

    std::string url = "http://lyrics.kugou.com/search?ver=1&man=yes&client=pc&keyword=" +
        encodeUrl(searchKeyword) + "&duration=200000&hash=" + encodeUrl(data.songId);

    std::string response;
    std::vector<std::string> headers = {
        "Referer: http://www.kugou.com",
        "Cookie: kg_mid=" + generateRandomMid()
    };

    if (!performHttpRequest(url, response, headers)) {
        return false;
    }

    try {
        json j = json::parse(response);
        if (j.contains("candidates") && !j["candidates"].empty()) {
            auto& candidate = j["candidates"][0];
            std::string id = candidate["id"].get<std::string>();
            std::string accesskey = candidate["accesskey"].get<std::string>();

            std::string downloadUrl = "http://lyrics.kugou.com/download?ver=1&client=pc&id=" +
                id + "&accesskey=" + accesskey + "&fmt=lrc&charset=utf8";

            std::string lyricResponse;
            if (performHttpRequest(downloadUrl, lyricResponse, headers)) {
                json lyricData = json::parse(lyricResponse);
                if (lyricData.contains("content")) {
                    std::string base64Content = lyricData["content"].get<std::string>();
                    std::string originalLyrics = base64Decode(base64Content);

                    data.lyrics = originalLyrics;

                    if (lyricData.contains("trans")) {
                        std::string base64Trans = lyricData["trans"].get<std::string>();
                        if (!base64Trans.empty()) {
                            std::string transLyric = base64Decode(base64Trans);
                            if (!transLyric.empty()) {
                                std::cout << "获取到酷狗翻译歌词(LRC)" << std::endl;
                                data.translationLyrics = transLyric;
                                data.hasTranslation = true;
                            }
                        }
                    }

                    if (!data.hasTranslation) {
                        std::string krcUrl = "http://lyrics.kugou.com/download?ver=1&client=pc&id=" +
                            id + "&accesskey=" + accesskey + "&fmt=krc&charset=utf8";
                        std::string krcResponse;
                        if (performHttpRequest(krcUrl, krcResponse, headers)) {
                            try {
                                json krcData = json::parse(krcResponse);
                                if (krcData.contains("content")) {
                                    std::string krcBase64 = krcData["content"].get<std::string>();
                                    std::string krcRaw = base64Decode(krcBase64);
                                    std::vector<uint8_t> krcBytes(krcRaw.begin(), krcRaw.end());

                                    std::string krcText, krcTrans;
                                    if (decryptKRC(krcBytes, krcText, krcTrans)) {
                                        std::string transFromKRC = extractTranslationFromKRCMetadata(krcText, originalLyrics);
                                        if (!transFromKRC.empty()) {
                                            std::cout << "获取到酷狗翻译歌词(KRC)" << std::endl;
                                            data.translationLyrics = transFromKRC;
                                            data.hasTranslation = true;
                                        }
                                    }
                                }
                            } catch (const json::exception& e) {
                                std::cerr << "KRC歌词JSON解析错误: " << e.what() << std::endl;
                            }
                        }
                    }

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


// 封面选取

std::vector<uint8_t> LyricsEncapsulator::getBestCoverFromAllPlatforms(
    const std::vector<std::unique_ptr<MusicData>>& allData,
    const std::string& specifiedSource) {
    
    std::vector<std::future<std::pair<std::string, std::vector<uint8_t>>>> coverFutures;
    
    std::cout << "\n开始从各平台下载封面..." << std::endl;
    std::cout << "指定数据源: " << specifiedSource << std::endl;
    
    // 为每个平台启动封面下载任务
    for (const auto& data : allData) {
        if (!data->coverUrl.empty()) {
            coverFutures.push_back(std::async(std::launch::async, [this, data = data.get()]() {
                std::cout << "正在从" << data->source << "下载封面..." << std::endl;
                auto coverData = downloadImageFromUrl(data->coverUrl);
                if (!coverData.empty()) {
                    std::cout << data->source << "封面下载成功: " << coverData.size() << " 字节" << std::endl;
                    return std::make_pair(data->source, coverData);
                } else {
                    std::cout << data->source << "封面下载失败" << std::endl;
                    return std::make_pair(std::string(), std::vector<uint8_t>());
                }
            }));
        }
    }
    
    // 收集所有封面数据
    std::vector<std::pair<std::string, std::vector<uint8_t>>> allCovers;
    for (auto& future : coverFutures) {
        auto result = future.get();
        if (!result.first.empty() && !result.second.empty()) {
            allCovers.push_back(result);
        }
    }
    
    if (allCovers.empty()) {
        std::cout << "所有平台封面下载失败" << std::endl;
        return {};
    }
    
    // 根据指定源选择封面
    std::vector<uint8_t> selectedCover;
    std::string selectedSource;
    
    if (specifiedSource == "酷狗音乐" || specifiedSource == "酷狗") {
        // 酷狗：优先选择酷狗封面，找不到才选最大的
        auto it = std::find_if(allCovers.begin(), allCovers.end(),
            [](const auto& cover) {
                return cover.first == "酷狗音乐";
            });
        
        if (it != allCovers.end()) {
            selectedCover = it->second;
            selectedSource = it->first;
            std::cout << "找到指定平台封面: " << selectedSource 
                      << " (" << selectedCover.size() << " 字节)" << std::endl;
        } else {
            auto bestCover = std::max_element(allCovers.begin(), allCovers.end(),
                [](const auto& a, const auto& b) {
                    return a.second.size() < b.second.size();
                });
            
            selectedCover = bestCover->second;
            selectedSource = bestCover->first;
            std::cout << "指定平台酷狗没有封面，选择最大封面: " 
                      << selectedSource << " (" << selectedCover.size() << " 字节)" << std::endl;
        }
                  
    } else if (specifiedSource == "QQ音乐" || specifiedSource == "QQ") {
        // QQ音乐：先尝试选择QQ音乐封面
        auto it = std::find_if(allCovers.begin(), allCovers.end(),
            [](const auto& cover) {
                return cover.first == "QQ音乐";
            });
        
        if (it != allCovers.end()) {
            // 找到QQ音乐的封面
            selectedCover = it->second;
            selectedSource = it->first;
            std::cout << "找到指定平台封面: " << selectedSource 
                      << " (" << selectedCover.size() << " 字节)" << std::endl;
        } else {
            // QQ音乐没有封面，选择最大的
            auto bestCover = std::max_element(allCovers.begin(), allCovers.end(),
                [](const auto& a, const auto& b) {
                    return a.second.size() < b.second.size();
                });
            
            selectedCover = bestCover->second;
            selectedSource = bestCover->first;
            std::cout << "指定平台QQ音乐没有封面，选择最大封面: " 
                      << selectedSource << " (" << selectedCover.size() << " 字节)" << std::endl;
        }
        
    } else if (specifiedSource == "网易云音乐" || specifiedSource == "网易云") {
        // 网易云：先尝试选择网易云封面
        auto it = std::find_if(allCovers.begin(), allCovers.end(),
            [](const auto& cover) {
                return cover.first == "网易云音乐";
            });
        
        if (it != allCovers.end()) {
            // 找到网易云的封面
            selectedCover = it->second;
            selectedSource = it->first;
            std::cout << "找到指定平台封面: " << selectedSource 
                      << " (" << selectedCover.size() << " 字节)" << std::endl;
        } else {
            // 网易云没有封面，选择最大的
            auto bestCover = std::max_element(allCovers.begin(), allCovers.end(),
                [](const auto& a, const auto& b) {
                    return a.second.size() < b.second.size();
                });
            
            selectedCover = bestCover->second;
            selectedSource = bestCover->first;
            std::cout << "指定平台网易云音乐没有封面，选择最大封面: " 
                      << selectedSource << " (" << selectedCover.size() << " 字节)" << std::endl;
        }
        
    } else {
        // 其他情况或默认选择最大的
        auto bestCover = std::max_element(allCovers.begin(), allCovers.end(),
            [](const auto& a, const auto& b) {
                return a.second.size() < b.second.size();
            });
        
        selectedCover = bestCover->second;
        selectedSource = bestCover->first;
        std::cout << "未识别指定源，选择最大封面: " << selectedSource 
                  << " (" << selectedCover.size() << " 字节)" << std::endl;
    }
    
    return selectedCover;
}

// ==================== 自动歌词偏移检测 ====================

