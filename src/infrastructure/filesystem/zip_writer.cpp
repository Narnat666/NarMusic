#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include "zip_writer.h"
#include <zlib.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <cstring>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <sys/stat.h>

namespace narnat {

struct CentralDirEntry {
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compression;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t filenameLen;
    uint16_t extraLen;
    uint16_t commentLen;
    uint16_t diskStart;
    uint16_t internalAttr;
    uint32_t externalAttr;
    uint32_t localHeaderOffset;
    std::string filename;
};

static uint16_t dosTime() {
    std::time_t now = std::time(nullptr);
    struct tm* t = std::localtime(&now);
    return static_cast<uint16_t>((t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2));
}

static uint16_t dosDate() {
    std::time_t now = std::time(nullptr);
    struct tm* t = std::localtime(&now);
    return static_cast<uint16_t>(((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday);
}

static bool readFileContents(const std::string& path, std::vector<char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    if (sz <= 0) { out.clear(); return true; }
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    f.read(out.data(), sz);
    return f.good() || f.eof();
}

static void writeU16(std::ostream& out, uint16_t v) {
    char buf[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
    out.write(buf, 2);
}

static void writeU32(std::ostream& out, uint32_t v) {
    char buf[4] = {
        static_cast<char>(v & 0xFF),
        static_cast<char>((v >> 8) & 0xFF),
        static_cast<char>((v >> 16) & 0xFF),
        static_cast<char>((v >> 24) & 0xFF)
    };
    out.write(buf, 4);
}

static uint32_t crc32Calc(const char* data, size_t len) {
    return static_cast<uint32_t>(::crc32(0L, reinterpret_cast<const Bytef*>(data), static_cast<uInt>(len)));
}

bool ZipWriter::createToFile(const std::vector<ZipEntry>& entries, const std::string& outputPath) {
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;

    std::vector<CentralDirEntry> centralDir;
    uint16_t modTime = dosTime();
    uint16_t modDate = dosDate();

    for (const auto& entry : entries) {
        std::vector<char> fileData;
        if (!readFileContents(entry.filePath, fileData)) {
            return false;
        }

        uint32_t uncompressedSize = static_cast<uint32_t>(fileData.size());
        uint32_t crc = 0;
        if (uncompressedSize > 0) {
            crc = crc32Calc(fileData.data(), fileData.size());
        }

        uint32_t compressedSize = 0;
        uint16_t compression = 0;
        std::vector<char> compressedData;

        if (uncompressedSize > 0) {
            z_stream strm = {};
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(fileData.data()));
            strm.avail_in = uncompressedSize;
            uLongf destLen = compressBound(static_cast<uLong>(uncompressedSize));
            compressedData.resize(destLen);
            strm.next_out = reinterpret_cast<Bytef*>(compressedData.data());
            strm.avail_out = static_cast<uInt>(destLen);

            if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                            -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) == Z_OK) {
                int ret = deflate(&strm, Z_FINISH);
                deflateEnd(&strm);
                if (ret == Z_STREAM_END) {
                    compressedData.resize(strm.total_out);
                    compressedSize = static_cast<uint32_t>(strm.total_out);
                    compression = 8;
                } else {
                    compressedData = std::move(fileData);
                    compressedSize = uncompressedSize;
                    compression = 0;
                }
            } else {
                compressedData = std::move(fileData);
                compressedSize = uncompressedSize;
                compression = 0;
            }
            fileData.clear();
            fileData.shrink_to_fit();
        }

        uint32_t localHeaderOffset = static_cast<uint32_t>(out.tellp());

        bool hasNonAscii = false;
        for (char c : entry.filename) {
            if (static_cast<unsigned char>(c) > 127) {
                hasNonAscii = true;
                break;
            }
        }

        uint16_t flags = hasNonAscii ? 0x0800 : 0;
        uint16_t versionNeeded = 20;

        writeU32(out, 0x04034b50);
        writeU16(out, versionNeeded);
        writeU16(out, flags);
        writeU16(out, compression);
        writeU16(out, modTime);
        writeU16(out, modDate);
        writeU32(out, crc);
        writeU32(out, compressedSize);
        writeU32(out, uncompressedSize);
        writeU16(out, static_cast<uint16_t>(entry.filename.size()));
        writeU16(out, 0);
        out.write(entry.filename.data(), static_cast<std::streamsize>(entry.filename.size()));
        out.write(compressedData.data(), static_cast<std::streamsize>(compressedData.size()));

        CentralDirEntry cd;
        cd.versionMadeBy = 20;
        cd.versionNeeded = versionNeeded;
        cd.flags = flags;
        cd.compression = compression;
        cd.modTime = modTime;
        cd.modDate = modDate;
        cd.crc32 = crc;
        cd.compressedSize = compressedSize;
        cd.uncompressedSize = uncompressedSize;
        cd.filenameLen = static_cast<uint16_t>(entry.filename.size());
        cd.extraLen = 0;
        cd.commentLen = 0;
        cd.diskStart = 0;
        cd.internalAttr = 0;
        cd.externalAttr = 0;
        cd.localHeaderOffset = localHeaderOffset;
        cd.filename = entry.filename;
        centralDir.push_back(cd);
    }

    uint32_t centralDirOffset = static_cast<uint32_t>(out.tellp());

    for (const auto& cd : centralDir) {
        writeU32(out, 0x02014b50);
        writeU16(out, cd.versionMadeBy);
        writeU16(out, cd.versionNeeded);
        writeU16(out, cd.flags);
        writeU16(out, cd.compression);
        writeU16(out, cd.modTime);
        writeU16(out, cd.modDate);
        writeU32(out, cd.crc32);
        writeU32(out, cd.compressedSize);
        writeU32(out, cd.uncompressedSize);
        writeU16(out, cd.filenameLen);
        writeU16(out, cd.extraLen);
        writeU16(out, cd.commentLen);
        writeU16(out, cd.diskStart);
        writeU16(out, cd.internalAttr);
        writeU32(out, cd.externalAttr);
        writeU32(out, cd.localHeaderOffset);
        out.write(cd.filename.data(), static_cast<std::streamsize>(cd.filename.size()));
    }

    uint32_t centralDirSize = static_cast<uint32_t>(out.tellp()) - centralDirOffset;

    writeU32(out, 0x06054b50);
    writeU16(out, 0);
    writeU16(out, 0);
    writeU16(out, static_cast<uint16_t>(centralDir.size()));
    writeU16(out, static_cast<uint16_t>(centralDir.size()));
    writeU32(out, centralDirSize);
    writeU32(out, centralDirOffset);
    writeU16(out, 0);

    out.close();
    return out.good();
}

} // namespace narnat
