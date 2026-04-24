#include "kugou_provider.h"
#include "lyrics_aggregator.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"
#include <zlib.h>
#include <random>
#include <regex>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace narnat {

using json = nlohmann::json;

KugouProvider::KugouProvider(std::shared_ptr<CurlClient> httpClient)
    : httpClient_(std::move(httpClient)) {}

bool KugouProvider::fetch(const std::string& keyword, MusicMetadata& out) {
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

bool KugouProvider::searchSong(const std::string& keyword, MusicMetadata& data) {
    std::string url = "http://mobilecdn.kugou.com/api/v3/search/song?format=json&keyword=" +
        httpClient_->urlEncode(keyword) + "&page=1&pagesize=10&showtype=1";

    auto resp = httpClient_->get(url);
    if (!resp.success) return false;

    try {
        json j = json::parse(resp.body);
        if (j.contains("status") && j["status"].get<int>() == 1 &&
            j.contains("data") && j["data"].contains("info") && !j["data"]["info"].empty()) {

            auto& songs = j["data"]["info"];
            size_t bestIndex = 0;

            if (songs.size() > 1) {
                long long maxPop = 0;
                for (size_t i = 0; i < songs.size(); i++) {
                    long long pop = 0;
                    if (songs[i].contains("play_count") && songs[i]["play_count"].is_number())
                        pop = songs[i]["play_count"].get<long long>();
                    if (pop > maxPop) maxPop = pop;
                }

                double bestScore = -1.0;
                for (size_t i = 0; i < songs.size(); i++) {
                    std::string songName = songs[i].value("songname", "");
                    double sim = LyricsAggregator::nameSimilarity(keyword, songName);
                    double artistBonus = 0.0;
                    if (songs[i].contains("singername") && songs[i]["singername"].is_string())
                        artistBonus = 0.2 * LyricsAggregator::nameSimilarity(keyword, songs[i]["singername"].get<std::string>());
                    double popBonus = 0.0;
                    if (maxPop > 0 && songs[i].contains("play_count") && songs[i]["play_count"].is_number())
                        popBonus = 0.15 * static_cast<double>(songs[i]["play_count"].get<long long>()) / static_cast<double>(maxPop);
                    double score = sim + artistBonus + popBonus;
                    if (score > bestScore) { bestScore = score; bestIndex = i; }
                }
            }

            auto& song = songs[bestIndex];
            if (song.contains("hash")) data.songId = song["hash"].get<std::string>();
            if (song.contains("songname")) data.songName = song["songname"].get<std::string>();
            if (song.contains("singername")) data.artist = song["singername"].get<std::string>();
            if (song.contains("album_name")) { data.album = song["album_name"].get<std::string>(); data.hasAlbum = true; }

            // 封面URL
            if (song.contains("album_img") && !song["album_img"].is_null()) {
                std::string albumImg = song["album_img"].get<std::string>();
                if (!albumImg.empty() && albumImg.find("http") == 0) {
                    albumImg.erase(std::remove(albumImg.begin(), albumImg.end(), ' '), albumImg.end());
                    if (albumImg.find("_150.jpg") != std::string::npos)
                        data.coverUrl = std::regex_replace(albumImg, std::regex("_150\\.jpg$"), "_500.jpg");
                    else
                        data.coverUrl = albumImg;
                }
            }
            if (song.contains("img") && !song["img"].is_null()) {
                std::string imgUrl = song["img"].get<std::string>();
                if (!imgUrl.empty() && imgUrl.find("http") == 0) {
                    imgUrl.erase(std::remove(imgUrl.begin(), imgUrl.end(), ' '), imgUrl.end());
                    data.coverUrl = imgUrl;
                }
            }
            if (data.coverUrl.empty() && song.contains("album_id")) {
                std::string albumId;
                if (song["album_id"].is_string()) albumId = song["album_id"].get<std::string>();
                else if (song["album_id"].is_number()) albumId = std::to_string(song["album_id"].get<long long>());
                if (!albumId.empty()) data.coverUrl = fetchAlbumCover(albumId);
            }
            return true;
        }
    } catch (const std::exception& e) {
        LOG_W("Kugou", std::string("搜索解析失败: ") + e.what());
    }
    return false;
}

std::string KugouProvider::fetchAlbumCover(const std::string& albumId) {
    std::string url = "http://mobilecdn.kugou.com/api/v3/album/info?albumid=" + albumId;
    auto resp = httpClient_->get(url);
    if (!resp.success) return "";

    try {
        json j = json::parse(resp.body);
        if (j.contains("status") && j["status"].get<int>() == 1 && j.contains("data")) {
            auto& d = j["data"];
            if (d.contains("imgurl") && !d["imgurl"].is_null()) {
                std::string coverUrl = d["imgurl"].get<std::string>();
                if (!coverUrl.empty() && coverUrl.find("http") == 0) {
                    coverUrl.erase(std::remove(coverUrl.begin(), coverUrl.end(), ' '), coverUrl.end());
                    size_t pos = coverUrl.find("{size}");
                    if (pos != std::string::npos) coverUrl.replace(pos, 6, "800");
                    return coverUrl;
                }
            }
            if (d.contains("sizable_cover") && !d["sizable_cover"].is_null()) {
                std::string coverUrl = d["sizable_cover"].get<std::string>();
                if (!coverUrl.empty()) {
                    coverUrl.erase(std::remove(coverUrl.begin(), coverUrl.end(), ' '), coverUrl.end());
                    size_t pos = coverUrl.find("{size}");
                    if (pos != std::string::npos) coverUrl.replace(pos, 6, "800");
                    return coverUrl;
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_W("Kugou", std::string("专辑封面解析失败: ") + e.what());
    }
    return "";
}

bool KugouProvider::getLyrics(MusicMetadata& data) {
    if (data.songId.empty() && data.songName.empty()) return false;

    std::string searchKeyword = !data.artist.empty() && !data.songName.empty()
        ? data.artist + " - " + data.songName
        : (!data.songName.empty() ? data.songName : data.songId);

    // 生成随机mid
    const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::string mid;
    for (int i = 0; i < 32; ++i) mid += hex[dis(gen)];

    std::string url = "http://lyrics.kugou.com/search?ver=1&man=yes&client=pc&keyword=" +
        httpClient_->urlEncode(searchKeyword) + "&duration=200000&hash=" + httpClient_->urlEncode(data.songId);

    auto resp = httpClient_->get(url, {"Referer: http://www.kugou.com", "Cookie: kg_mid=" + mid});
    if (!resp.success) return false;

    try {
        json j = json::parse(resp.body);
        if (!j.contains("candidates") || j["candidates"].empty()) return false;

        auto& candidate = j["candidates"][0];
        std::string id = candidate["id"].get<std::string>();
        std::string accesskey = candidate["accesskey"].get<std::string>();

        // LRC格式
        std::string dlUrl = "http://lyrics.kugou.com/download?ver=1&client=pc&id=" +
            id + "&accesskey=" + accesskey + "&fmt=lrc&charset=utf8";

        auto dlResp = httpClient_->get(dlUrl, {"Referer: http://www.kugou.com", "Cookie: kg_mid=" + mid});
        if (!dlResp.success) return false;

        json lyricData = json::parse(dlResp.body);
        if (!lyricData.contains("content")) return false;

        std::string base64Content = lyricData["content"].get<std::string>();
        // base64解码
        auto b64Decode = [](const std::string& encoded) -> std::string {
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
        };

        std::string originalLyrics = b64Decode(base64Content);
        data.lyrics = originalLyrics;

        // 翻译歌词（LRC）
        if (lyricData.contains("trans")) {
            std::string base64Trans = lyricData["trans"].get<std::string>();
            if (!base64Trans.empty()) {
                std::string transLyric = b64Decode(base64Trans);
                if (!transLyric.empty()) {
                    data.translationLyrics = transLyric;
                    data.hasTranslation = true;
                }
            }
        }

        // 如果没有翻译，尝试KRC格式
        if (!data.hasTranslation) {
            std::string krcUrl = "http://lyrics.kugou.com/download?ver=1&client=pc&id=" +
                id + "&accesskey=" + accesskey + "&fmt=krc&charset=utf8";
            auto krcResp = httpClient_->get(krcUrl, {"Referer: http://www.kugou.com", "Cookie: kg_mid=" + mid});
            if (krcResp.success) {
                try {
                    json krcJson = json::parse(krcResp.body);
                    if (krcJson.contains("content")) {
                        std::string krcBase64 = krcJson["content"].get<std::string>();
                        std::string krcRaw = b64Decode(krcBase64);
                        std::vector<uint8_t> krcBytes(krcRaw.begin(), krcRaw.end());
                        std::string krcText, krcTrans;
                        if (decryptKRC(krcBytes, krcText, krcTrans)) {
                            if (!krcTrans.empty()) {
                                data.translationLyrics = krcTrans;
                                data.hasTranslation = true;
                            }
                        }
                    }
                } catch (...) {}
            }
        }

        data.hasLyrics = !data.lyrics.empty();
        return data.hasLyrics;
    } catch (const std::exception& e) {
        LOG_W("Kugou", std::string("歌词获取失败: ") + e.what());
    }
    return false;
}

bool KugouProvider::decryptKRC(const std::vector<uint8_t>& krcData, std::string& outLyrics, std::string& outTranslation) {
    if (krcData.size() < 8) return false;
    if (krcData[0] != 'k' || krcData[1] != 'r' || krcData[2] != 'c' || krcData[3] != '1') return false;

    static const uint8_t xorKey[] = {64, 71, 97, 119, 94, 50, 116, 71, 81, 54, 49, 45, 206, 210, 110, 105};
    std::vector<uint8_t> decrypted(krcData.size() - 4);
    for (size_t i = 4; i < krcData.size(); i++)
        decrypted[i - 4] = krcData[i] ^ xorKey[(i - 4) % 16];

    uLongf destLen = decrypted.size() * 10;
    std::vector<uint8_t> decompressed(destLen);
    int ret = uncompress(decompressed.data(), &destLen, decrypted.data(), decrypted.size());
    if (ret != Z_OK) return false;
    decompressed.resize(destLen);

    outLyrics = std::string(decompressed.begin(), decompressed.end());
    outTranslation.clear();
    return true;
}

} // namespace narnat
