#include "qishui_provider.h"
#include "lyrics_aggregator.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"
#include <regex>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace narnat {

using json = nlohmann::json;

QishuiProvider::QishuiProvider(std::shared_ptr<CurlClient> httpClient)
    : httpClient_(std::move(httpClient)) {}

bool QishuiProvider::fetch(const std::string& keyword, MusicMetadata& out) {
    QishuiContext ctx;

    if (!searchSong(keyword, out, ctx)) return false;

    if (!getLyrics(ctx, out)) {
        out.hasLyrics = false;
    }

    if (!ctx.coverUrl.empty()) {
        auto resp = httpClient_->download(ctx.coverUrl, {"Accept: image/*,*/*;q=0.8"});
        if (resp.success && !resp.binaryBody.empty() && resp.binaryBody.size() >= 5120) {
            out.coverData = std::move(resp.binaryBody);
            out.hasCover = true;
            out.coverSize = static_cast<int>(out.coverData.size());
        }
    }

    return true;
}

bool QishuiProvider::searchSong(const std::string& keyword, MusicMetadata& data, QishuiContext& ctx) {
    std::string url = "https://api.qishui.com/luna/pc/search/track?"
        "aid=386088&app_name=luna_pc&region=cn&geo_region=cn&os_region=cn"
        "&device_id=1088932190113307&iid=2332504177791808"
        "&version_name=3.0.0&version_code=30000000&channel=official"
        "&device_platform=windows&device_type=Windows"
        "&q=" + httpClient_->urlEncode(keyword) +
        "&cursor=0&search_method=input";

    std::vector<std::string> headers = {
        "User-Agent: LunaPC/3.0.0(290101097)",
        "Content-Type: application/json; charset=utf-8",
        "X-Helios: SicAACJWDNiSHEX4DSBVXo3+TNXAHXt9Af6CkPaMTmSX1Jcg",
        "X-Medusa: GBR+aez8IZPMw6JSzT62GUzbH3GODwMBg7ZESAAAAQk/GduVkAZWRUCTX0SGSLVDDQ/gYOFKM/adGsI29F3FyR/OAoj+AK7fOY1Pe1po0w3w850g3Y0xvZOEl35RaWIynTM+dvmKmsQLoBG2LPT9eoaLqF8pi6MjvRdIJK8PMnnwDYrreh4OQ85zqzZdCFytOf6cXPH4NImgdUgBceuFfUtCN8ZdI3bRTDD28J8OxDK8vsWjdzimSPNTIe6C2EKel/U+PcqXfkbs/ZWCvHyxmqgrLfu5tHAtnXuEbQf6J53G8I6wdY8JQ5wm8+7o37XUiWC8FCB6y+09/aB9q4LTwNEMOlv50fAQg/bT9RgB6+7jF+7RXZyIuNkXAuJb2uZeBSzfJVvw6VITls5AFSOdNu376GqKGm4T6M8V9HzT2L8cW8smYgNG6HJPjd3iVVcv8fjeJeAGolEPMBbBvbAjJCSQAOY6jo/RGbRvOUsDyZgJ6fEp8ncjXIcK6Nw1GSPOv7AXWILqyt5sBpFDvPlpJTqih5TWbmWSEBc52+OPX2DJKknmz4qBPrRdJ7QvtxA5nrLDBjc3doDJa2iv1FE/7nUQoGJ5njCFw2BYfT9LE3kxDVUtWzmYLtxzkFGpuhGdAuRYSSC2LiCgbGcaqIkDrUpa2yaVZNimFJi3s08+OCUllT5aQQIh/mv02EEXGXi1IV7UCWqTNEdzjZrat6P2rNQbG0DYXvj3sbTJX8+7mS/c6LD5sWZ4UjKiVo4PMRknYHv3syjwX4VuvF49u/+fHYWtv72Y+buTO0iuGDxIiOk6kNElV895F40J6WpZ59nPpg7Qum8ndQHko5xqtdAXIB/l//3/v+/3//8AAA==",
    };

    auto resp = httpClient_->get(url, headers);
    if (!resp.success) {
        LOG_W("Qishui", "搜索请求失败");
        return false;
    }

    try {
        json j = json::parse(resp.body);

        if (!j.contains("result_groups") || !j["result_groups"].is_array() || j["result_groups"].empty()) {
            LOG_W("Qishui", "搜索无结果");
            return false;
        }

        auto& groups = j["result_groups"];
        auto& firstGroup = groups[0];
        if (!firstGroup.contains("data") || !firstGroup["data"].is_array() || firstGroup["data"].empty()) {
            LOG_W("Qishui", "搜索结果数据为空");
            return false;
        }

        auto& songs = firstGroup["data"];
        size_t bestIndex = 0;
        double bestScore = -1.0;

        for (size_t i = 0; i < songs.size(); i++) {
            auto* track = &songs[i];
            if (songs[i].contains("entity") && songs[i]["entity"].contains("track")) {
                track = &songs[i]["entity"]["track"];
            }

            std::string songName = track->value("name", "");
            std::string artistStr;
            if (track->contains("artists") && track->at("artists").is_array()) {
                for (auto& a : track->at("artists")) {
                    if (a.contains("name") && a["name"].is_string()) {
                        if (!artistStr.empty()) artistStr += " ";
                        artistStr += a["name"].get<std::string>();
                    }
                }
            }

            double sim = LyricsAggregator::nameSimilarity(keyword, songName);
            double artistBonus = 0.2 * LyricsAggregator::nameSimilarity(keyword, artistStr);
            double score = sim + artistBonus;

            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }

        auto* bestTrack = &songs[bestIndex];
        if (songs[bestIndex].contains("entity") && songs[bestIndex]["entity"].contains("track")) {
            bestTrack = &songs[bestIndex]["entity"]["track"];
        }

        ctx.trackId = bestTrack->value("id", "");
        if (ctx.trackId.empty()) {
            LOG_W("Qishui", "track_id为空");
            return false;
        }

        ctx.trackName = bestTrack->value("name", "");
        ctx.artistName.clear();
        if (bestTrack->contains("artists") && bestTrack->at("artists").is_array()) {
            for (auto& a : bestTrack->at("artists")) {
                if (a.contains("name") && a["name"].is_string()) {
                    if (!ctx.artistName.empty()) ctx.artistName += ", ";
                    ctx.artistName += a["name"].get<std::string>();
                }
            }
        }

        if (bestTrack->contains("album") && bestTrack->at("album").is_object()) {
            auto& album = bestTrack->at("album");
            data.album = album.value("name", "");
            data.hasAlbum = !data.album.empty();

            if (album.contains("url_cover") && album["url_cover"].is_object()) {
                auto& cover = album["url_cover"];
                std::string coverBase;
                if (cover.contains("urls") && cover["urls"].is_array() && !cover["urls"].empty()) {
                    coverBase = cover["urls"][0].get<std::string>();
                }
                std::string uri = cover.value("uri", "");
                if (!coverBase.empty() || !uri.empty()) {
                    ctx.coverUrl = coverBase + uri + "~c5_1080x1080.jpg";
                }
            }
        }

        data.songName = ctx.trackName;
        data.artist = ctx.artistName;

        LOG_I("Qishui", "搜索命中: " + ctx.trackName + " - " + ctx.artistName + " (id=" + ctx.trackId + ")");
        return true;

    } catch (const std::exception& e) {
        LOG_W("Qishui", std::string("搜索解析失败: ") + e.what());
    }
    return false;
}

std::string QishuiProvider::extractJsonObject(const std::string& html, const std::string& fieldName) {
    std::string marker = "\"" + fieldName + "\"";
    size_t idx = html.find(marker);
    if (idx == std::string::npos) return "";

    size_t objStart = html.find('{', idx);
    if (objStart == std::string::npos || objStart > idx + marker.size() + 5) return "";

    int brace = 0;
    size_t objEnd = objStart;
    for (size_t i = objStart; i < std::min(objStart + 200000, html.size()); i++) {
        if (html[i] == '{') brace++;
        else if (html[i] == '}') {
            brace--;
            if (brace == 0) { objEnd = i + 1; break; }
        }
    }

    if (objEnd <= objStart) return "";
    return html.substr(objStart, objEnd - objStart);
}

bool QishuiProvider::getLyrics(QishuiContext& ctx, MusicMetadata& data) {
    if (ctx.trackId.empty()) return false;

    std::string url = "https://music.douyin.com/qishui/share/track?track_id=" + ctx.trackId;

    auto resp = httpClient_->get(url, {
        "Referer: https://music.douyin.com/",
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "User-Agent: Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.6 Mobile/15E148 Safari/604.1",
    });

    if (!resp.success || resp.body.empty()) {
        LOG_W("Qishui", "分享页面请求失败");
        return false;
    }

    std::string awoJson = extractJsonObject(resp.body, "audioWithLyricsOption");
    if (awoJson.empty()) {
        awoJson = extractJsonObject(resp.body, "audioWithLyrics");
    }
    if (awoJson.empty()) {
        LOG_W("Qishui", "未找到audioWithLyricsOption");
        return false;
    }

    try {
        json awo = json::parse(awoJson);

        std::string ssTrackName = awo.value("trackName", "");
        std::string ssArtist = awo.value("artistName", "");

        if (!ssTrackName.empty()) {
            double sim = LyricsAggregator::nameSimilarity(ctx.trackName, ssTrackName);
            if (sim < 0.2) {
                LOG_W("Qishui", "歌曲匹配度低: " + ctx.trackName + " vs " + ssTrackName);
                return false;
            }
            data.songName = ssTrackName;
        }
        if (!ssArtist.empty()) data.artist = ssArtist;

        if (!awo.contains("lyrics") || !awo["lyrics"].is_object()) {
            LOG_W("Qishui", "无歌词数据");
            return false;
        }

        auto& lyrics = awo["lyrics"];
        if (!lyrics.contains("sentences") || !lyrics["sentences"].is_array()) {
            LOG_W("Qishui", "歌词sentences为空");
            return false;
        }

        std::string lyricsJson = lyrics.dump();
        data.lyrics = krcToLrc(lyricsJson);
        data.hasLyrics = !data.lyrics.empty();

        std::string ssCoverUrl = awo.value("coverURL", "");
        if (!ssCoverUrl.empty() && ssCoverUrl.find("http") == 0) {
            size_t tildePos = ssCoverUrl.find("~");
            if (tildePos != std::string::npos)
                ssCoverUrl = ssCoverUrl.substr(0, tildePos) + "~c5_1080x1080.jpg";
            ctx.coverUrl = ssCoverUrl;
        }

        LOG_I("Qishui", "歌词获取成功: " + data.songName + " (" + std::to_string(lyrics["sentences"].size()) + "行)");
        return data.hasLyrics;

    } catch (const std::exception& e) {
        LOG_W("Qishui", std::string("歌词解析失败: ") + e.what());
    }
    return false;
}

std::string QishuiProvider::krcToLrc(const std::string& krcJson) {
    try {
        json krc = json::parse(krcJson);
        if (!krc.contains("sentences") || !krc["sentences"].is_array()) return "";

        std::ostringstream lrc;
        auto& sentences = krc["sentences"];

        for (auto& s : sentences) {
            if (!s.is_object()) continue;

            int64_t startMs = 0;
            if (s.contains("startMs") && s["startMs"].is_number()) {
                startMs = s["startMs"].get<int64_t>();
            }
            if (startMs == 0 && s.contains("words") && s["words"].is_array() && !s["words"].empty()) {
                if (s["words"][0].contains("startMs") && s["words"][0]["startMs"].is_number()) {
                    startMs = s["words"][0]["startMs"].get<int64_t>();
                }
            }

            std::string text;
            if (s.contains("words") && s["words"].is_array() && !s["words"].empty()) {
                for (auto& w : s["words"]) {
                    if (w.contains("text") && w["text"].is_string()) {
                        text += w["text"].get<std::string>();
                    }
                }
            }
            if (text.empty() && s.contains("text") && s["text"].is_string()) {
                text = s["text"].get<std::string>();
            }

            if (text.empty()) continue;

            int min = static_cast<int>(startMs / 60000);
            int sec = static_cast<int>((startMs % 60000) / 1000);
            int ms = static_cast<int>(startMs % 1000);

            lrc << "[" << std::setfill('0') << std::setw(2) << min
                << ":" << std::setw(2) << sec
                << "." << std::setw(3) << ms << "]"
                << text << "\n";
        }

        return lrc.str();
    } catch (const std::exception& e) {
        LOG_W("Qishui", std::string("KRC转LRC失败: ") + e.what());
    }
    return "";
}

} // namespace narnat
