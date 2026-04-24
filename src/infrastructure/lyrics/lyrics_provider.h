#ifndef NARNAT_LYRICS_PROVIDER_H
#define NARNAT_LYRICS_PROVIDER_H

#include <string>
#include <memory>
#include "domain/music_metadata.h"
#include "infrastructure/http_client/curl_client.h"

namespace narnat {

class ILyricsProvider {
public:
    virtual ~ILyricsProvider() = default;

    virtual std::string name() const = 0;

    // 搜索歌曲并获取歌词+封面
    virtual bool fetch(const std::string& keyword, MusicMetadata& out) = 0;
};

} // namespace narnat

#endif
