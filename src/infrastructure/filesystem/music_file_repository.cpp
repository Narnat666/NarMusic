#include "music_file_repository.h"
#include "core/logger.h"
#include <filesystem>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <taglib/mp4/mp4file.h>
#include <taglib/mp4/mp4tag.h>
#include <taglib/mp4/mp4coverart.h>
#include <taglib/mp4/mp4item.h>
#pragma GCC diagnostic pop
#include <cstdio>

namespace narnat {

bool FsMusicFileRepository::fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

long long FsMusicFileRepository::fileSize(const std::string& path) {
    try { return static_cast<long long>(std::filesystem::file_size(path)); } catch (...) { return -1; }
}

bool FsMusicFileRepository::deleteFile(const std::string& path) {
    try { return std::filesystem::remove(path); } catch (...) { return false; }
}

bool FsMusicFileRepository::ensureDirectory(const std::string& path) {
    try { std::filesystem::create_directories(path); return true; } catch (...) { return false; }
}

bool FsMusicFileRepository::writeMetadata(const std::string& filePath,
                                           const MusicMetadata& metadata) {
    fflush(stderr);
    FILE* oldStderr = freopen("/dev/null", "w", stderr);

    TagLib::MP4::File file(filePath.c_str());

    if (!file.isValid()) {
        if (oldStderr) freopen("/dev/tty", "w", stderr);
        LOG_E("FsMusicRepo", "无法打开M4A文件: " + filePath);
        return false;
    }

    auto* tag = file.tag();
    if (!tag) {
        if (oldStderr) freopen("/dev/tty", "w", stderr);
        return false;
    }

    if (!metadata.songName.empty()) {
        tag->setTitle(TagLib::String(metadata.songName, TagLib::String::UTF8));
    }

    if (!metadata.artist.empty()) {
        tag->setArtist(TagLib::String(metadata.artist, TagLib::String::UTF8));
    }

    if (!metadata.album.empty()) {
        tag->setAlbum(TagLib::String(metadata.album, TagLib::String::UTF8));
    }

    if (!metadata.lyrics.empty()) {
        tag->setItem("\xA9lyr",
            TagLib::MP4::Item(TagLib::String(metadata.lyrics, TagLib::String::UTF8)));
    }

    if (metadata.delayMs != 0) {
        tag->setItem("----:com.narnat:delayMs",
            TagLib::MP4::Item(TagLib::String(std::to_string(metadata.delayMs), TagLib::String::UTF8)));
    }

    if (!metadata.narmeta.empty()) {
        tag->setItem("----:com.narnat:narmeta",
            TagLib::MP4::Item(TagLib::String(metadata.narmeta, TagLib::String::UTF8)));
    }

    if (!metadata.coverData.empty()) {
        TagLib::MP4::CoverArt::Format format = TagLib::MP4::CoverArt::JPEG;
        if (metadata.coverData.size() > 8) {
            if (metadata.coverData[0] == 0x89 && metadata.coverData[1] == 0x50 &&
                metadata.coverData[2] == 0x4E && metadata.coverData[3] == 0x47)
                format = TagLib::MP4::CoverArt::PNG;
        }
        TagLib::MP4::CoverArtList covers;
        covers.append(TagLib::MP4::CoverArt(format,
            TagLib::ByteVector(
                reinterpret_cast<const char*>(metadata.coverData.data()),
                static_cast<unsigned int>(metadata.coverData.size()))));
        tag->setItem("covr", TagLib::MP4::Item(covers));
    }

    if (!file.save()) {
        if (oldStderr) freopen("/dev/tty", "w", stderr);
        LOG_E("FsMusicRepo", "M4A保存失败: " + filePath);
        return false;
    }

    if (oldStderr) freopen("/dev/tty", "w", stderr);

    LOG_I("FsMusicRepo", "元数据写入成功: " + filePath);
    return true;
}

} // namespace narnat
