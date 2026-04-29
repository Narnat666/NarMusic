#ifndef NARNAT_LYRICS_AGGREGATOR_H
#define NARNAT_LYRICS_AGGREGATOR_H

#include <vector>
#include <memory>
#include <string>
#include "lyrics_provider.h"
#include "domain/music_metadata.h"
#include "domain/repository/ilyrics_aggregator.h"

namespace narnat {

class LyricsAggregator : public ILyricsAggregator {
public:
    void addProvider(std::shared_ptr<ILyricsProvider> provider);

    MusicMetadata fetchBest(const std::string& keyword,
                            const std::string& preferredPlatform = "") override;

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

    static bool hasCoreData(const MusicMetadata& d);
    static bool isComplete(const MusicMetadata& d);
    static int missingScore(const MusicMetadata& d);
    static std::vector<std::string> buildPlatformOrder(const std::string& preferredPlatform);
    static const MusicMetadata* findPlatform(const std::vector<MusicMetadata>& allData,
                                               const std::string& platform);
    static void supplementMissing(MusicMetadata& best,
                                    const std::vector<MusicMetadata>& allData,
                                    const std::vector<std::string>& platformOrder);
};

} // namespace narnat

#endif
