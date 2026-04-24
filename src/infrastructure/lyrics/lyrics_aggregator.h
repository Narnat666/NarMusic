#ifndef NARNAT_LYRICS_AGGREGATOR_H
#define NARNAT_LYRICS_AGGREGATOR_H

#include <vector>
#include <memory>
#include <string>
#include "lyrics_provider.h"
#include "domain/music_metadata.h"

namespace narnat {

class LyricsAggregator {
public:
    void addProvider(std::shared_ptr<ILyricsProvider> provider);

    // 并发从所有平台获取，选择最佳歌词+封面
    MusicMetadata fetchBest(const std::string& keyword,
                            const std::string& preferredPlatform = "");

    // 歌词时间偏移
    static std::string adjustLyricsTiming(const std::string& lyrics, int offsetMs);

    // 双语合并
    static std::string mergeBilingualLyrics(const std::string& original,
                                             const std::string& translation);

    // 歌词清洗
    static std::string cleanLyrics(const std::string& lyrics);

    // LRC格式标准化
    static std::string convertLRCToStandardFormat(const std::string& lrcText);

    // 歌名相似度
    static double nameSimilarity(const std::string& a, const std::string& b);

private:
    std::vector<std::shared_ptr<ILyricsProvider>> providers_;

    // 选择最佳封面（按文件大小）
    std::vector<uint8_t> getBestCover(
        const std::vector<MusicMetadata>& allData,
        const std::string& preferredSource);

    // 选择最佳歌词（按平台优先级）
    std::pair<std::string, bool> getBestLyrics(
        const std::vector<MusicMetadata>& allData,
        const std::string& preferredPlatform);
};

} // namespace narnat

#endif
