#ifndef LYRICS_ENCAPSULATOR_H
#define LYRICS_ENCAPSULATOR_H

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

// 为m4a音乐文件写入歌词和封面
// 需要文件名和文件路径作为输入
// 次要输入为歌词获取平台、专辑封面获取平台、歌词与歌曲速率调整

class LyricsEncapsulator {
    
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
            int coverSize = 0;
        };
        bool searchSongFromNetease(const std::string& keyword, MusicData& data); // 网易云获取数据
        bool performHttpRequest(const std::string& url, 
            std::string& response,
            const std::vector<std::string>& extraHeaders = {},
            const std::string& postData = "",
            long timeout = 15); // http请求函数 
        std::string adjustLyricsTiming(const std::string& lyrics, int offsetMs); // 延迟设置
        bool searchSongFromKugou(const std::string& keyword, MusicData& data);
        bool searchSongFromQQMusic(const std::string& keyword, MusicData& data);
        bool getLyricsFromKugou(MusicData& data, int offsetMs);
        bool getLyricsFromQQMusic(MusicData& data, int offsetMs);
        bool getLyricsFromNetease(MusicData& data, int offsetMs);
        std::vector<uint8_t> getBestCoverFromAllPlatforms(
            const std::vector<std::unique_ptr<MusicData>>& allData,
            const std::string& specifiedSource);
        std::pair<std::string, bool> getBestLyrics(const std::vector<std::unique_ptr<MusicData>>& allData, const std::string platform);
        bool writeToM4AFile(const std::string& filepath, const MusicData& data);
        std::string encodeUrl(const std::string& value);
        bool setMP4CoverArt(TagLib::MP4::File& file, const std::vector<uint8_t>& coverData);
        bool setMP4Lyrics(TagLib::MP4::File& file, const std::string& lyrics);
        std::string cleanLyrics(const std::string& lyrics);
        std::string convertLRCToStandardFormat(const std::string& lrcText);
        std::string generateRandomMid();
        std::vector<uint8_t> downloadImageFromUrl(const std::string& url);
        bool isValidImage(const std::vector<uint8_t>& data);
        std::string base64Decode(const std::string& encoded);
    
    public:
        LyricsEncapsulator();
        ~LyricsEncapsulator();
        bool updateMusicMetadata(const std::string& songName, const std::string& m4aFilePath, const std::string& platform, const int offsetMs);
};

#endif