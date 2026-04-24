#include "netease_provider.h"
#include "lyrics_aggregator.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"
#include <algorithm>

namespace narnat {

using json = nlohmann::json;

NeteaseProvider::NeteaseProvider(std::shared_ptr<CurlClient> httpClient)
    : httpClient_(std::move(httpClient)) {}

bool NeteaseProvider::fetch(const std::string& keyword, MusicMetadata& out) {
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

bool NeteaseProvider::searchSong(const std::string& keyword, MusicMetadata& data) {
    std::string url = "http://music.163.com/api/cloudsearch/pc?s=" +
        httpClient_->urlEncode(keyword) + "&type=1&offset=0&limit=10";

    auto resp = httpClient_->get(url, {"Referer: http://music.163.com", "Origin: http://music.163.com"});
    if (!resp.success) return false;

    try {
        json j = json::parse(resp.body);
        if (j.contains("code") && j["code"].get<int>() == 200 &&
            j.contains("result") && j["result"].contains("songs") && !j["result"]["songs"].empty()) {

            auto& songs = j["result"]["songs"];
            int bestIndex = 0;

            if (songs.size() > 1) {
                long long maxPop = 0;
                for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                    long long pop = songs[i].value("pop", 0LL);
                    if (pop > maxPop) maxPop = pop;
                }

                double bestScore = -1.0;
                for (int i = 0; i < static_cast<int>(songs.size()); i++) {
                    std::string songName = songs[i].value("name", "");
                    double sim = LyricsAggregator::nameSimilarity(keyword, songName);

                    double artistBonus = 0.0;
                    if (songs[i].contains("ar") && !songs[i]["ar"].empty() && songs[i]["ar"][0].contains("name"))
                        artistBonus = 0.2 * LyricsAggregator::nameSimilarity(keyword, songs[i]["ar"][0]["name"].get<std::string>());

                    double popBonus = 0.0;
                    if (maxPop > 0 && songs[i].contains("pop") && songs[i]["pop"].is_number())
                        popBonus = 0.15 * static_cast<double>(songs[i]["pop"].get<long long>()) / maxPop;

                    double score = sim + artistBonus + popBonus;
                    if (score > bestScore) { bestScore = score; bestIndex = i; }
                }
            }

            auto& song = songs[bestIndex];
            data.songId = std::to_string(song["id"].get<long long>());
            if (song.contains("name")) data.songName = song["name"].get<std::string>();
            if (song.contains("ar") && !song["ar"].empty() && song["ar"][0].contains("name"))
                data.artist = song["ar"][0]["name"].get<std::string>();
            if (song.contains("al") && song["al"].contains("name")) {
                data.album = song["al"]["name"].get<std::string>();
                data.hasAlbum = true;
            }
            if (song.contains("al") && !song["al"].is_null() && song["al"].contains("picUrl")) {
                std::string picUrl = song["al"]["picUrl"].get<std::string>();
                size_t pos = picUrl.find("?");
                if (pos != std::string::npos) picUrl = picUrl.substr(0, pos);
                data.coverUrl = picUrl + "?param=1000y1000";
            }
            return true;
        }
    } catch (const std::exception& e) {
        LOG_W("Netease", std::string("搜索解析失败: ") + e.what());
    }
    return false;
}

bool NeteaseProvider::getLyrics(MusicMetadata& data) {
    std::string url = "http://music.163.com/api/song/lyric?id=" + data.songId + "&lv=-1&kv=-1&tv=-1";

    auto resp = httpClient_->get(url, {"Referer: http://music.163.com", "Origin: http://music.163.com"});
    if (!resp.success) return false;

    try {
        json j = json::parse(resp.body);
        if (j.contains("lrc") && j["lrc"].contains("lyric") && !j["lrc"]["lyric"].get<std::string>().empty()) {
            data.lyrics = j["lrc"]["lyric"].get<std::string>();
            data.hasLyrics = true;

            if (j.contains("tlyric") && j["tlyric"].contains("lyric") && !j["tlyric"]["lyric"].is_null()) {
                std::string transLyric = j["tlyric"]["lyric"].get<std::string>();
                if (!transLyric.empty()) {
                    data.translationLyrics = transLyric;
                    data.hasTranslation = true;
                }
            }
            return true;
        }
    } catch (const std::exception& e) {
        LOG_W("Netease", std::string("歌词获取失败: ") + e.what());
    }
    return false;
}

} // namespace narnat
