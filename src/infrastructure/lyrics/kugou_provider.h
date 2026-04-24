#ifndef NARNAT_KUGOU_PROVIDER_H
#define NARNAT_KUGOU_PROVIDER_H

#include "lyrics_provider.h"

namespace narnat {

class KugouProvider : public ILyricsProvider {
public:
    explicit KugouProvider(std::shared_ptr<CurlClient> httpClient);

    std::string name() const override { return "酷狗音乐"; }
    bool fetch(const std::string& keyword, MusicMetadata& out) override;

private:
    bool searchSong(const std::string& keyword, MusicMetadata& data);
    bool getLyrics(MusicMetadata& data);
    std::string fetchAlbumCover(const std::string& albumId);
    bool decryptKRC(const std::vector<uint8_t>& krcData, std::string& outLyrics,
                    std::string& outTranslation);

    std::shared_ptr<CurlClient> httpClient_;
};

} // namespace narnat

#endif
