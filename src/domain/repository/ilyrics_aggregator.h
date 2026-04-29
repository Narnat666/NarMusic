#ifndef NARNAT_ILYRICS_AGGREGATOR_H
#define NARNAT_ILYRICS_AGGREGATOR_H

#include <string>
#include <memory>
#include "domain/music_metadata.h"

namespace narnat {

class ILyricsAggregator {
public:
    virtual ~ILyricsAggregator() = default;

    virtual MusicMetadata fetchBest(const std::string& keyword,
                                    const std::string& preferredPlatform = "") = 0;
};

} // namespace narnat

#endif
