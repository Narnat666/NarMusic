#ifndef NARNAT_NETEASE_PROVIDER_H
#define NARNAT_NETEASE_PROVIDER_H

#include "lyrics_provider.h"

namespace narnat {

class NeteaseProvider : public ILyricsProvider {
public:
    explicit NeteaseProvider(std::shared_ptr<CurlClient> httpClient);

    std::string name() const override { return "网易云音乐"; }
    bool fetch(const std::string& keyword, MusicMetadata& out) override;

private:
    bool searchSong(const std::string& keyword, MusicMetadata& data);
    bool getLyrics(MusicMetadata& data);

    std::shared_ptr<CurlClient> httpClient_;
};

} // namespace narnat

#endif
