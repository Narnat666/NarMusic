#ifndef NARNAT_QQMUSIC_PROVIDER_H
#define NARNAT_QQMUSIC_PROVIDER_H

#include "lyrics_provider.h"

namespace narnat {

class QQMusicProvider : public ILyricsProvider {
public:
    explicit QQMusicProvider(std::shared_ptr<CurlClient> httpClient);

    std::string name() const override { return "QQ音乐"; }
    bool fetch(const std::string& keyword, MusicMetadata& out) override;

private:
    bool searchSong(const std::string& keyword, MusicMetadata& data);
    bool getLyrics(MusicMetadata& data);
    std::string generateRandomMid();
    std::string base64Decode(const std::string& encoded);

    std::shared_ptr<CurlClient> httpClient_;
};

} // namespace narnat

#endif
