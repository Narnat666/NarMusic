#ifndef NARNAT_QISHUI_PROVIDER_H
#define NARNAT_QISHUI_PROVIDER_H

#include "lyrics_provider.h"
#include "infrastructure/http_client/curl_client.h"

namespace narnat {

struct QishuiContext {
    std::string trackId;
    std::string trackName;
    std::string artistName;
    std::string coverUrl;
};

class QishuiProvider : public ILyricsProvider {
public:
    explicit QishuiProvider(std::shared_ptr<CurlClient> httpClient);

    std::string name() const override { return "汽水音乐"; }
    bool fetch(const std::string& keyword, MusicMetadata& out) override;

private:
    bool searchSong(const std::string& keyword, MusicMetadata& data, QishuiContext& ctx);
    bool getLyrics(QishuiContext& ctx, MusicMetadata& data);
    bool getLyricsViaApi(QishuiContext& ctx, MusicMetadata& data);
    bool getLyricsViaSharePage(QishuiContext& ctx, MusicMetadata& data);
    std::string krcStringToLrc(const std::string& krcContent);
    std::pair<std::string, std::string> krcJsonToLrc(const std::string& krcJson);
    std::string extractJsonObject(const std::string& html, const std::string& fieldName);

    std::shared_ptr<CurlClient> httpClient_;
};

} // namespace narnat

#endif
