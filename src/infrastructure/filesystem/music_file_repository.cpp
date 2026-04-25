#include "music_file_repository.h"
#include "core/logger.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <taglib/mp4/mp4file.h>
#include <taglib/mp4/mp4tag.h>
#include <taglib/mp4/mp4coverart.h>
#include <taglib/mp4/mp4item.h>
#include <cstdio>

namespace narnat {

std::vector<MusicFileInfo> FsMusicFileRepository::scanLibrary(const std::string& directory) {
    std::vector<MusicFileInfo> files;
    namespace fs = std::filesystem;

    if (!fs::exists(directory)) return files;

    static const std::vector<std::string> musicExts = {
        ".m4a", ".mp3", ".flac", ".wav", ".aac", ".ogg"
    };

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        bool isMusic = false;
        for (const auto& e : musicExts) {
            if (ext == e) { isMusic = true; break; }
        }
        if (!isMusic) continue;

        MusicFileInfo info;
        info.systemFilename = entry.path().filename().string();
        info.customFilename = info.systemFilename;
        info.fileSize = static_cast<long long>(fs::file_size(entry.path()));

        auto ftime = fs::last_write_time(entry.path());
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        info.downloadTime = std::chrono::system_clock::to_time_t(sctp);

        // 从M4A元数据读取延迟和自定义文件名
        if (ext == ".m4a") {
            FILE* oldStderr2 = nullptr;
            try {
                // 抑制TagLib的stderr警告
                fflush(stderr);
                oldStderr2 = freopen("/dev/null", "w", stderr);

                TagLib::MP4::File m4aFile(entry.path().c_str());

                if (m4aFile.isValid()) {
                    auto* tag = m4aFile.tag();
                    if (tag) {
                        // 读取延迟
                        auto delayKey = tag->itemMap().find("----:com.narnat:delayMs");
                        if (delayKey != tag->itemMap().end()) {
                            try {
                                info.delayMs = std::stoi(delayKey->second.toStringList().front().toCString(true));
                            } catch (...) {}
                        }
                        // 读取标题作为自定义文件名
                        auto title = tag->title();
                        if (!title.isEmpty()) {
                            info.customFilename = title.toCString(true) + ext;
                        }
                    }
                }
            } catch (...) {}
            if (oldStderr2) freopen("/dev/tty", "w", stderr);
        }

        files.push_back(info);
    }

    std::sort(files.begin(), files.end(), [](const MusicFileInfo& a, const MusicFileInfo& b) {
        return a.downloadTime > b.downloadTime;
    });

    return files;
}

bool FsMusicFileRepository::fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

long long FsMusicFileRepository::fileSize(const std::string& path) {
    try { return static_cast<long long>(std::filesystem::file_size(path)); } catch (...) { return -1; }
}

std::string FsMusicFileRepository::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    return std::string(std::istreambuf_iterator<char>(file), {});
}

bool FsMusicFileRepository::deleteFile(const std::string& path) {
    try { return std::filesystem::remove(path); } catch (...) { return false; }
}

bool FsMusicFileRepository::ensureDirectory(const std::string& path) {
    try { std::filesystem::create_directories(path); return true; } catch (...) { return false; }
}

bool FsMusicFileRepository::writeMetadata(const std::string& filePath,
                                           const MusicMetadata& metadata) {
    // 抑制TagLib的stderr警告(如"Invalid atom size")
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

    // 写入标题
    if (!metadata.songName.empty()) {
        tag->setTitle(TagLib::String(metadata.songName, TagLib::String::UTF8));
    }

    // 写入艺术家
    if (!metadata.artist.empty()) {
        tag->setArtist(TagLib::String(metadata.artist, TagLib::String::UTF8));
    }

    // 写入专辑
    if (!metadata.album.empty()) {
        tag->setAlbum(TagLib::String(metadata.album, TagLib::String::UTF8));
    }

    // 写入歌词
    if (!metadata.lyrics.empty()) {
        tag->setItem("\xA9lyr",
            TagLib::MP4::Item(TagLib::String(metadata.lyrics, TagLib::String::UTF8)));
    }

    // 写入延迟信息到自定义tag
    if (metadata.delayMs != 0) {
        tag->setItem("----:com.narnat:delayMs",
            TagLib::MP4::Item(TagLib::String(std::to_string(metadata.delayMs), TagLib::String::UTF8)));
    }

    // 写入封面
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

    // 恢复stderr
    if (oldStderr) freopen("/dev/tty", "w", stderr);

    LOG_I("FsMusicRepo", "元数据写入成功: " + filePath);
    return true;
}

} // namespace narnat
