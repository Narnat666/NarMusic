#include "qqmusic_provider.h"
#include "lyrics_aggregator.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"
#include <random>
#include <algorithm>

namespace narnat {

using json = nlohmann::json;

QQMusicProvider::QQMusicProvider(std::shared_ptr<CurlClient> httpClient)
    : httpClient_(std::move(httpClient)) {}

bool QQMusicProvider::fetch(const std::string& keyword, MusicMetadata& out) {
    if (!searchSong(keyword, out)) return false;
    if (!getLyrics(out)) {
        out.hasLyrics = false;
    }
    // 下载封面
    if (!out.coverUrl.empty()) {
        auto resp = httpClient_->download(out.coverUrl, {"Accept: image/*,*/*;q=0.8"});
        if (resp.success && !resp.binaryBody.empty() && resp.binaryBody.size() >= 5120) {
            out.coverData = std::move(resp.binaryBody);
            out.hasCover = true;
            out.coverSize = static_cast<int>(out.coverData.size());
        }
    }
    return true;
}

std::string QQMusicProvider::generateRandomMid() {
    const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::string mid;
    for (int i = 0; i < 32; ++i) mid += hex[dis(gen)];
    return mid;
}

std::string QQMusicProvider::base64Decode(const std::string& encoded) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    int val = 0, bits = -8;
    for (unsigned char c : encoded) {
        if (c == '=') break;
        if (std::isspace(c)) continue;
        size_t idx = chars.find(c);
        if (idx == std::string::npos) break;
        val = (val << 6) + static_cast<int>(idx);
        bits += 6;
        if (bits >= 0) { decoded.push_back(static_cast<char>((val >> bits) & 0xFF)); bits -= 8; }
    }
    return decoded;
}

bool QQMusicProvider::searchSong(const std::string& keyword, MusicMetadata& data) {
    json reqJson;
    reqJson["comm"] = {{"ct", 19}, {"cv", "1859"}, {"uin", "0"}};
    reqJson["req"]["method"] = "DoSearchForQQMusicDesktop";
    reqJson["req"]["module"] = "music.search.SearchCgiService";
    reqJson["req"]["param"]["query"] = keyword;
    reqJson["req"]["param"]["num_per_page"] = 10;
    reqJson["req"]["param"]["page_num"] = 1;

    std::string jsonData = reqJson.dump();
    std::string url = "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json";

    auto resp = httpClient_->post(url, jsonData,
        {"Referer: https://y.qq.com", "Origin: https://y.qq.com", "Content-Type: application/json"});

    if (!resp.success) {
        // 降级GET
        std::string getUrl = url + "&data=" + httpClient_->urlEncode(jsonData);
        resp = httpClient_->get(getUrl, {"Referer: https://y.qq.com", "Origin: https://y.qq.com"});
    }

    if (!resp.success || resp.body.empty()) return false;

    try {
        json j = json::parse(resp.body);
        json* songList = nullptr;

        if (j.contains("req") && j["req"].contains("data") &&
            j["req"]["data"].contains("body") &&
            j["req"]["data"]["body"].contains("song") &&
            j["req"]["data"]["body"]["song"].contains("list")) {
            songList = &j["req"]["data"]["body"]["song"]["list"];
        }

        if (!songList || songList->empty()) return false;

        auto& songs = *songList;
        size_t bestIndex = 0;

        if (songs.size() > 1) {
            long long maxPop = 0;
            for (size_t i = 0; i < songs.size(); i++) {
                long long pop = songs[i].value("listenCnt", 0LL);
                if (pop > maxPop) maxPop = pop;
            }

            double bestScore = -1.0;
            for (size_t i = 0; i < songs.size(); i++) {
                std::string songName = songs[i].value("name", "");
                double sim = LyricsAggregator::nameSimilarity(keyword, songName);

                double artistBonus = 0.0;
                if (songs[i].contains("singer") && songs[i]["singer"].is_array() &&
                    !songs[i]["singer"].empty() && songs[i]["singer"][0].contains("name"))
                    artistBonus = 0.2 * LyricsAggregator::nameSimilarity(keyword, songs[i]["singer"][0]["name"].get<std::string>());

                double popBonus = 0.0;
                if (maxPop > 0 && songs[i].contains("listenCnt") && songs[i]["listenCnt"].is_number())
                    popBonus = 0.15 * static_cast<double>(songs[i]["listenCnt"].get<long long>()) / static_cast<double>(maxPop);

                double score = sim + artistBonus + popBonus;
                if (score > bestScore) { bestScore = score; bestIndex = i; }
            }
        }

        auto& song = songs[bestIndex];
        if (song.contains("mid") && song["mid"].is_string()) data.songId = song["mid"].get<std::string>();
        if (song.contains("name") && song["name"].is_string()) data.songName = song["name"].get<std::string>();
        if (song.contains("singer") && song["singer"].is_array() && !song["singer"].empty() &&
            song["singer"][0].contains("name") && song["singer"][0]["name"].is_string())
            data.artist = song["singer"][0]["name"].get<std::string>();
        if (song.contains("album") && song["album"].is_object()) {
            if (song["album"].contains("name") && song["album"]["name"].is_string()) {
                data.album = song["album"]["name"].get<std::string>();
                data.hasAlbum = true;
            }
            if (song["album"].contains("mid") && song["album"]["mid"].is_string()) {
                std::string albumMid = song["album"]["mid"].get<std::string>();
                if (!albumMid.empty())
                    data.coverUrl = "https://y.gtimg.cn/music/photo_new/T002R800x800M000" + albumMid + ".jpg";
            }
        }
        return true;
    } catch (const std::exception& e) {
        LOG_W("QQMusic", std::string("搜索解析失败: ") + e.what());
    }
    return false;
}

bool QQMusicProvider::getLyrics(MusicMetadata& data) {
    json reqJson;
    reqJson["comm"] = {{"ct", 19}, {"cv", "1859"}, {"uin", "0"}};
    reqJson["req"]["method"] = "GetPlayLyricInfo";
    reqJson["req"]["module"] = "music.musichallSong.PlayLyricInfo";
    reqJson["req"]["param"]["songMID"] = data.songId;
    reqJson["req"]["param"]["trans"] = 1;

    std::string jsonData = reqJson.dump();
    std::string url = "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json";

    auto resp = httpClient_->post(url, jsonData,
        {"Referer: https://y.qq.com", "Origin: https://y.qq.com", "Content-Type: application/json"});

    if (!resp.success) {
        std::string getUrl = url + "&data=" + httpClient_->urlEncode(jsonData);
        resp = httpClient_->get(getUrl, {"Referer: https://y.qq.com", "Origin: https://y.qq.com"});
    }

    if (!resp.success || resp.body.empty()) return false;

    try {
        json j = json::parse(resp.body);
        if (!j.contains("req") || !j["req"].contains("data")) return false;

        auto& lyricData = j["req"]["data"];
        std::string originalLyrics;
        if (lyricData.contains("lyric") && lyricData["lyric"].is_string()) {
            std::string base64Lyric = lyricData["lyric"].get<std::string>();
            if (!base64Lyric.empty()) originalLyrics = base64Decode(base64Lyric);
        }

        if (originalLyrics.empty()) return false;

        if (lyricData.contains("trans") && lyricData["trans"].is_string()) {
            std::string base64Trans = lyricData["trans"].get<std::string>();
            if (!base64Trans.empty()) {
                std::string transLyric = base64Decode(base64Trans);
                if (!transLyric.empty()) {
                    data.translationLyrics = transLyric;
                    data.hasTranslation = true;
                }
            }
        }

        data.lyrics = originalLyrics;
        data.hasLyrics = true;
        return true;
    } catch (const std::exception& e) {
        LOG_W("QQMusic", std::string("歌词获取失败: ") + e.what());
    }
    return false;
}

} // namespace narnat
