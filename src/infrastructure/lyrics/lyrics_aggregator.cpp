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

bool LyricsAggregator::hasCoreData(const MusicMetadata& d) {
    return d.hasLyrics && d.hasTranslation && !d.translationLyrics.empty();
}

bool LyricsAggregator::isComplete(const MusicMetadata& d) {
    return hasCoreData(d) && d.hasCover;
}

int LyricsAggregator::missingScore(const MusicMetadata& d) {
    int score = 0;
    if (hasCoreData(d)) score += 100;
    if (!d.album.empty()) score += 10;
    if (d.hasCover) score += 1;
    return score;
}

std::vector<std::string> LyricsAggregator::buildPlatformOrder(const std::string& preferredPlatform) {
    static const std::vector<std::string> defaultOrder = {"酷狗音乐", "网易云音乐", "QQ音乐", "汽水音乐"};
    if (preferredPlatform.empty()) return defaultOrder;

    std::vector<std::string> order;
    order.push_back(preferredPlatform);
    for (auto& p : defaultOrder) {
        if (p != preferredPlatform) order.push_back(p);
    }
    return order;
}

const MusicMetadata* LyricsAggregator::findPlatform(const std::vector<MusicMetadata>& allData,
                                                      const std::string& platform) {
    for (auto& d : allData) {
        if (d.source == platform) return &d;
    }
    return nullptr;
}

void LyricsAggregator::supplementMissing(MusicMetadata& best,
                                           const std::vector<MusicMetadata>& allData,
                                           const std::vector<std::string>& platformOrder) {
    if (!best.hasCover) {
        for (auto& d : allData) {
            if (d.hasCover && !d.coverData.empty()) {
                best.coverData = d.coverData;
                best.hasCover = true;
                best.coverSize = d.coverSize;
                LOG_I("LyricsAgg", "补封面: 从 " + d.source + " 补充");
                break;
            }
        }
    }

    if (best.album.empty()) {
        for (auto& d : allData) {
            if (!d.album.empty()) {
                best.album = d.album;
                best.hasAlbum = true;
                LOG_I("LyricsAgg", "补专辑: 从 " + d.source + " 补充");
                break;
            }
        }
    }

    if (best.artist.empty()) {
        for (auto& d : allData) {
            if (!d.artist.empty()) { best.artist = d.artist; break; }
        }
    }
    if (best.songName.empty()) {
        for (auto& d : allData) {
            if (!d.songName.empty()) { best.songName = d.songName; break; }
        }
    }

    if (!best.hasLyrics) {
        for (auto& plat : platformOrder) {
            auto* d = findPlatform(allData, plat);
            if (d && d->hasLyrics) {
                best.lyrics = cleanLyrics(d->lyrics);
                best.hasLyrics = true;
                LOG_W("LyricsAgg", "补歌词: 从 " + d->source + " 补充");
                if (d->hasTranslation && !d->translationLyrics.empty()) {
                    best.translationLyrics = d->translationLyrics;
                    best.hasTranslation = true;
                    LOG_I("LyricsAgg", "补翻译: 与歌词同源 " + d->source);
                }
                break;
            }
        }
    }

    if (best.hasLyrics && !best.hasTranslation) {
        for (auto& plat : platformOrder) {
            auto* d = findPlatform(allData, plat);
            if (d && d->hasTranslation && !d->translationLyrics.empty()) {
                best.translationLyrics = d->translationLyrics;
                best.hasTranslation = true;
                LOG_W("LyricsAgg", "补翻译: 从 " + d->source + " 补充 (可能与歌词不同源)");
                break;
            }
        }
    }
}

MusicMetadata LyricsAggregator::fetchBest(const std::string& keyword,
                                           const std::string& preferredPlatform) {
    if (providers_.empty()) {
        LOG_W("LyricsAgg", "无歌词提供者");
        return {};
    }

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

    auto platformOrder = buildPlatformOrder(preferredPlatform);

    // 第一级：指定平台有歌词+翻译 → 直接选，缺封面/专辑从其他平台补
    if (!preferredPlatform.empty()) {
        auto* d = findPlatform(allData, preferredPlatform);
        if (d && hasCoreData(*d)) {
            LOG_I("LyricsAgg", "第一级: 指定平台 " + preferredPlatform + " 有歌词+翻译，直接选取");
            MusicMetadata best = *d;
            if (!best.lyrics.empty()) best.lyrics = cleanLyrics(best.lyrics);
            supplementMissing(best, allData, platformOrder);
            return best;
        }
        LOG_W("LyricsAgg", "第一级: 指定平台 " + preferredPlatform + " 缺少歌词或翻译，跳过");
    }

    // 第二级：按平台优先级找第一个完整平台（歌词+翻译+封面）
    for (auto& plat : platformOrder) {
        auto* d = findPlatform(allData, plat);
        if (d && isComplete(*d)) {
            LOG_I("LyricsAgg", "第二级: 整体选取平台 " + plat + " (完整)");
            MusicMetadata best = *d;
            if (!best.lyrics.empty()) best.lyrics = cleanLyrics(best.lyrics);
            supplementMissing(best, allData, platformOrder);
            return best;
        }
    }

    // 第三级：兜底拼接——所有平台都不完整
    LOG_W("LyricsAgg", "第三级: 所有平台均不完整，启用兜底拼接策略");

    const MusicMetadata* primary = nullptr;
    int bestScore = -1;
    for (auto& plat : platformOrder) {
        auto* d = findPlatform(allData, plat);
        if (!d) continue;
        int score = missingScore(*d);
        if (score > bestScore) {
            bestScore = score;
            primary = d;
        }
    }

    if (!primary) {
        LOG_E("LyricsAgg", "所有平台均无有效数据");
        MusicMetadata empty;
        empty.songName = keyword;
        return empty;
    }

    MusicMetadata best = *primary;
    if (!best.lyrics.empty()) best.lyrics = cleanLyrics(best.lyrics);

    LOG_I("LyricsAgg", "拼接主平台: " + best.source + " (歌词="
          + std::string(best.hasLyrics ? "Y" : "N")
          + " 翻译=" + std::string(best.hasTranslation ? "Y" : "N")
          + " 封面=" + std::string(best.hasCover ? "Y" : "N")
          + " 专辑=" + std::string(!best.album.empty() ? "Y" : "N") + ")");

    supplementMissing(best, allData, platformOrder);
    return best;
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
        std::string msStr = (*it)[3].str();
        int ms = std::stoi(msStr);
        if (msStr.length() == 2) ms *= 10;
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
