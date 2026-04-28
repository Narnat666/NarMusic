#ifndef NARNAT_MUSIC_LIBRARY_ENTRY_H
#define NARNAT_MUSIC_LIBRARY_ENTRY_H

#include <string>
#include <chrono>
#include <cstdint>
#include "nlohmann/json.hpp"

namespace narnat {

struct MusicLibraryEntry {
    int id = 0;
    std::string songName;
    std::string artist;
    std::string album;
    std::string filePath;
    std::string systemFilename;
    std::string originalFilename;
    int64_t fileSize = 0;
    std::string platform;
    int delayMs = 0;
    bool inUse = false;
    std::chrono::system_clock::time_point downloadedAt;
    std::chrono::system_clock::time_point lastUsedAt;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["custom_filename"] = songName.empty() ? systemFilename : songName;
        j["original_filename"] = originalFilename;
        j["system_filename"] = systemFilename;
        j["artist"] = artist;
        j["album"] = album;
        j["platform"] = platform;
        j["file_size"] = fileSize;
        j["delay_ms"] = delayMs;
        j["download_time"] = std::chrono::system_clock::to_time_t(downloadedAt);
        j["last_used_time"] = std::chrono::system_clock::to_time_t(lastUsedAt);
        return j;
    }
};

} // namespace narnat

#endif
