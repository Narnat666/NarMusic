#include "lyrics_encapsulator.h"

using json = nlohmann::json;

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
            data.lyrics = j["lrc"]["lyric"].get<std::string>();
            data.hasLyrics = true;
            return true;
        }
    } catch (const json::exception& e) {
        std::cerr << "网易云歌词JSON解析错误: " << e.what() << std::endl;
    }
    
    return false;
}

bool LyricsEncapsulator:: searchSongFromKugou(const std::string& keyword, MusicData& data) {
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
            
            // 获取封面URL
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
            
            // 如果还没有封面URL，尝试其他字段
            if (data.coverUrl.empty() && song.contains("img") && !song["img"].is_null()) {
                std::string imgUrl = song["img"].get<std::string>();
                if (!imgUrl.empty() && imgUrl.find("http") == 0) {
                    imgUrl.erase(std::remove(imgUrl.begin(), imgUrl.end(), ' '), imgUrl.end());
                    data.coverUrl = imgUrl;
                }
            }
            
            return true;
        }
    } catch (const json::exception& e) {
        std::cerr << "酷狗搜索JSON解析错误: " << e.what() << std::endl;
    }
    
    return false;
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

bool LyricsEncapsulator::getLyricsFromKugou(MusicData& data, int offsetMs) {
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
                    data.lyrics = base64Decode(base64Content);
                    
                    // 酷狗歌词通常比实际音乐快 0.8-1.2 秒，这里添加 800ms 延迟
                    // 调整建议：
                    // - 如果歌词仍然比音乐快 → 增大数值（如 1000 或 1200）
                    // - 如果歌词比音乐慢了 → 减小数值（如 500 或 300）
                    if (!data.lyrics.empty()) {
                        std::cout << "offsetMs: " << offsetMs << std::endl;
                        data.lyrics = adjustLyricsTiming(data.lyrics, offsetMs);
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


bool LyricsEncapsulator::searchSongFromQQMusic(const std::string& keyword, MusicData& data) {
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
            
            // 获取封面URL
            if (song.contains("albummid")) {
                std::string albumMid = song["albummid"].get<std::string>();
                if (!albumMid.empty()) {
                    data.coverUrl = "https://y.gtimg.cn/music/photo_new/T002R800x800M000" + 
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

bool LyricsEncapsulator::getLyricsFromQQMusic(MusicData& data) {
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
            // 验证是有效的图片
            if (isValidImage(coverData)) {
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return coverData;
            }
        }
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return {};
}


std::vector<uint8_t> LyricsEncapsulator::getBestCoverFromAllPlatforms(const std::vector<std::unique_ptr<MusicData>>& allData) {
    std::vector<std::future<std::pair<std::string, std::vector<uint8_t>>>> coverFutures;
    
    std::cout << "\n开始从各平台下载封面..." << std::endl;
    
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
    
    // 选择最大的封面
    auto bestCover = std::max_element(allCovers.begin(), allCovers.end(),
        [](const auto& a, const auto& b) {
            return a.second.size() < b.second.size();
        });
    
    std::cout << "选择最大封面: " << bestCover->first 
              << " (" << bestCover->second.size() << " 字节)" << std::endl;
    
    return bestCover->second;
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
    // 按优先级搜索：酷狗 > 网易云 > QQ音乐
    std::vector<std::string> priority = {"酷狗音乐", "网易云音乐", "QQ音乐"};
    // 如果指定了平台
    if (!platform.empty()) priority[0] = platform;

    for (const auto& source : priority) {
        for (const auto& data : allData) {
            if (data->source == source && data->hasLyrics && !data->lyrics.empty()) {
                std::cout << "使用" << source << "的歌词" << std::endl;
                return {data->lyrics, true};
            }
        }
    }
    
    // 如果没有找到歌词，尝试从有歌词的平台获取
    for (const auto& data : allData) {
        if (data->hasLyrics && !data->lyrics.empty()) {
            std::cout << "使用" << data->source << "的歌词" << std::endl;
            return {data->lyrics, true};
        }
    }
    
    std::cout << "未找到歌词" << std::endl;
    return {"", false};
}

bool LyricsEncapsulator::updateMusicMetadata(const std::string& songName, const std::string& m4aFilePath,
                                             const std::string& platform, const int offsetMs 
                                                ) {
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
    futures.push_back(std::async(std::launch::async, [this, songName, offsetMs]() {
        auto data = std::make_unique<MusicData>();
        if (this->searchSongFromKugou(songName, *data)) {
            data->source = "酷狗音乐";
            // 获取歌词（优先）
            if (!this->getLyricsFromKugou(*data, offsetMs)) {
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
    auto bestCover = getBestCoverFromAllPlatforms(allData);
    
    // 获取最佳歌词（优先酷狗）
    auto lyricsResult = getBestLyrics(allData, platform);
    std::string bestLyrics = lyricsResult.first;
    bool hasLyrics = lyricsResult.second;
    
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