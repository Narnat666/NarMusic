#ifndef NARNAT_MUSIC_METADATA_H
#define NARNAT_MUSIC_METADATA_H

#include <string>
#include <vector>
#include <cstdint>

namespace narnat {

struct MusicMetadata {
    std::string source;           // 数据源（网易云/酷狗/QQ）
    std::string songId;           // 歌曲ID
    std::string songName;         // 歌名
    std::string artist;           // 艺术家
    std::string album;            // 专辑
    std::string lyrics;           // 歌词文本
    std::string translationLyrics;// 翻译歌词
    std::vector<uint8_t> coverData;// 封面二进制数据
    std::string coverUrl;         // 封面URL
    bool hasLyrics = false;
    bool hasTranslation = false;
    bool hasCover = false;
    bool hasAlbum = false;
    int coverSize = 0;
};

struct SearchResult {
    std::string title;
    std::string link;
};

} // namespace narnat

#endif
