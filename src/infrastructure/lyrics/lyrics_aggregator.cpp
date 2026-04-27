#include "lyrics_aggregator.h"
#include "core/logger.h"
#include <future>
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <map>
#include <set>

namespace narnat {

void LyricsAggregator::addProvider(std::shared_ptr<ILyricsProvider> provider) {
    providers_.push_back(std::move(provider));
}

MusicMetadata LyricsAggregator::fetchBest(const std::string& keyword,
                                           const std::string& preferredPlatform) {
    if (providers_.empty()) {
        LOG_W("LyricsAgg", "无歌词提供者");
        return {};
    }

    // 并发调用所有平台
    std::vector<std::future<MusicMetadata>> futures;
    for (auto& provider : providers_) {
        futures.push_back(std::async(std::launch::async, [&provider, &keyword]() {
            MusicMetadata data;
            data.source = provider->name();
            try {
                if (provider->fetch(keyword, data)) {
                    LOG_I("LyricsAgg", provider->name() + " 获取成功");
                } else {
                    LOG_W("LyricsAgg", provider->name() + " 获取失败");
                }
            } catch (const std::exception& e) {
                LOG_E("LyricsAgg", provider->name() + " 异常: " + std::string(e.what()));
            }
            return data;
        }));
    }

    std::vector<MusicMetadata> allData;
    for (auto& f : futures) {
        allData.push_back(f.get());
    }

    MusicMetadata best;
    best.songName = keyword;

    auto [lyrics, _] = getBestLyrics(allData, preferredPlatform);
    if (!lyrics.empty()) {
        best.lyrics = cleanLyrics(lyrics);
        best.hasLyrics = true;
    }

    best.coverData = getBestCover(allData, preferredPlatform);
    best.hasCover = !best.coverData.empty();

    best.translationLyrics = getBestTranslation(allData, preferredPlatform);
    best.hasTranslation = !best.translationLyrics.empty();

    for (auto& d : allData) {
        if (d.source == preferredPlatform) {
            if (!d.artist.empty()) best.artist = d.artist;
            if (!d.album.empty()) best.album = d.album;
            if (!d.songName.empty()) best.songName = d.songName;
        }
    }

    for (auto& d : allData) {
        if (!d.artist.empty() && best.artist.empty()) best.artist = d.artist;
        if (!d.album.empty() && best.album.empty()) best.album = d.album;
        if (!d.songName.empty() && best.songName == keyword) best.songName = d.songName;
    }

    return best;
}

std::vector<uint8_t> LyricsAggregator::getBestCover(
    const std::vector<MusicMetadata>& allData,
    const std::string& preferredSource) {

    const MusicMetadata* best = nullptr;
    for (auto& d : allData) {
        if (!d.hasCover) continue;
        if (!preferredSource.empty() && d.source == preferredSource) {
            best = &d;
            break;
        }
        if (!best || d.coverSize > best->coverSize) {
            best = &d;
        }
    }
    return best ? best->coverData : std::vector<uint8_t>{};
}

std::pair<std::string, bool> LyricsAggregator::getBestLyrics(
    const std::vector<MusicMetadata>& allData,
    const std::string& preferredPlatform) {

    // 优先使用指定平台
    if (!preferredPlatform.empty()) {
        for (auto& d : allData) {
            if (d.source == preferredPlatform && d.hasLyrics) {
                return {d.lyrics, d.hasTranslation};
            }
        }
    }

    // 按平台顺序选择
    static const std::vector<std::string> priority = {"酷狗音乐", "网易云音乐", "QQ音乐", "汽水音乐"};
    for (auto& plat : priority) {
        for (auto& d : allData) {
            if (d.source == plat && d.hasLyrics) {
                return {d.lyrics, d.hasTranslation};
            }
        }
    }

    return {"", false};
}

std::string LyricsAggregator::getBestTranslation(
    const std::vector<MusicMetadata>& allData,
    const std::string& preferredPlatform) {

    if (!preferredPlatform.empty()) {
        for (auto& d : allData) {
            if (d.source == preferredPlatform && d.hasTranslation && !d.translationLyrics.empty()) {
                return d.translationLyrics;
            }
        }
    }

    static const std::vector<std::string> priority = {"酷狗音乐", "网易云音乐", "QQ音乐", "汽水音乐"};
    for (auto& plat : priority) {
        for (auto& d : allData) {
            if (d.source == plat && d.hasTranslation && !d.translationLyrics.empty()) {
                return d.translationLyrics;
            }
        }
    }

    return "";
}

std::string LyricsAggregator::adjustLyricsTiming(const std::string& lyrics, int offsetMs) {
    if (offsetMs == 0 || lyrics.empty()) return lyrics;

    std::regex timeRegex(R"(\[(\d+):(\d+)\.(\d+)\])");
    std::string result;
    std::sregex_iterator it(lyrics.begin(), lyrics.end(), timeRegex);
    std::sregex_iterator end;
    size_t lastPos = 0;

    for (; it != end; ++it) {
        result += lyrics.substr(lastPos, static_cast<size_t>(it->position()) - lastPos);

        int min = std::stoi((*it)[1].str());
        int sec = std::stoi((*it)[2].str());
        int ms = std::stoi((*it)[3].str());
        long long totalMs = (min * 60 + sec) * 1000 + ms + offsetMs;
        if (totalMs < 0) totalMs = 0;

        auto newMin = static_cast<int>(totalMs / 60000);
        auto newSec = static_cast<int>((totalMs % 60000) / 1000);
        auto newMs = static_cast<int>(totalMs % 1000);

        std::ostringstream oss;
        oss << "[" << std::setfill('0') << std::setw(2) << newMin
            << ":" << std::setw(2) << newSec
            << "." << std::setw(2) << newMs / 10 << "]";
        result += oss.str();
        lastPos = static_cast<size_t>(it->position()) + static_cast<size_t>(it->length());
    }
    result += lyrics.substr(lastPos);
    return result;
}

std::string LyricsAggregator::mergeBilingualLyrics(const std::string& original,
                                                     const std::string& translation) {
    if (translation.empty()) return original;

    // 双时间戳格式（椒盐音乐标准）
    std::map<int, std::pair<std::string, std::string>> merged;
    std::vector<std::string> metaData;

    std::regex lrcLineRegex(R"(^\[(\d{2}):(\d{2})\.(\d{2,3})\](.*)$)");
    std::smatch match;

    auto parseLrc = [&](const std::string& text, bool isTranslation) {
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
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

                if (isTranslation) merged[timestamp].second = content;
                else merged[timestamp].first = content;
            } else if (!isTranslation && !line.empty() && line[0] == '[') {
                metaData.push_back(line);
            }
        }
    };

    parseLrc(original, false);
    parseLrc(translation, true);

    std::stringstream result;
    for (const auto& meta : metaData) result << meta << "\n";

    for (const auto& pair : merged) {
        int ts = pair.first;
        int mm = ts / 60000;
        int ss = (ts % 60000) / 1000;
        int xx = (ts % 1000) / 10;
        char tsStr[32];
        std::snprintf(tsStr, sizeof(tsStr), "[%02d:%02d.%02d]", mm, ss, xx);

        if (!pair.second.first.empty()) result << tsStr << pair.second.first << "\n";
        if (!pair.second.second.empty() && pair.second.second != pair.second.first)
            result << tsStr << pair.second.second << "\n";
    }
    return result.str();
}

std::string LyricsAggregator::cleanLyrics(const std::string& lyrics) {
    std::string result = lyrics;
    // 移除空时间标签行
    std::regex emptyLine(R"(\[\d+:\d+\.\d+\]\s*\n)");
    result = std::regex_replace(result, emptyLine, "");
    return result;
}

std::string LyricsAggregator::convertLRCToStandardFormat(const std::string& lrcText) {
    // 标准化时间标签格式 [mm:ss.xx]
    std::regex timeRegex(R"(\[(\d+):(\d+)[.:](\d+)\])");
    return std::regex_replace(lrcText, timeRegex, "[$1:$2.$3]");
}

double LyricsAggregator::nameSimilarity(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return 0.0;

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

    std::sort(wordsA.begin(), wordsA.end());
    std::sort(wordsB.begin(), wordsB.end());
    std::vector<std::string> intersection;
    std::set_intersection(wordsA.begin(), wordsA.end(), wordsB.begin(), wordsB.end(),
                          std::back_inserter(intersection));

    size_t minSize = std::min(wordsA.size(), wordsB.size());
    return static_cast<double>(intersection.size()) / static_cast<double>(minSize);
}

} // namespace narnat
