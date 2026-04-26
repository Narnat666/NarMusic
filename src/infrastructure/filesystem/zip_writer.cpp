// 在包含任何头文件之前抑制zlib的旧式类型转换警告
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

namespace narnat {

static void writeU16LE(std::vector<char>& buf, uint16_t v) {
    buf.push_back(static_cast<char>(v & 0xFF));
    buf.push_back(static_cast<char>((v >> 8) & 0xFF));
}

static void writeU32LE(std::vector<char>& buf, uint32_t v) {
    buf.push_back(static_cast<char>(v & 0xFF));
    buf.push_back(static_cast<char>((v >> 8) & 0xFF));
    buf.push_back(static_cast<char>((v >> 16) & 0xFF));
    buf.push_back(static_cast<char>((v >> 24) & 0xFF));
}

static uint32_t crc32Calc(const char* data, size_t len) {
    return static_cast<uint32_t>(::crc32(0L, reinterpret_cast<const Bytef*>(data), static_cast<uInt>(len)));
}

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

std::vector<char> ZipWriter::create(const std::vector<ZipEntry>& entries) {
    std::vector<char> result;
    std::vector<CentralDirEntry> centralDir;
    uint16_t modTime = dosTime();
    uint16_t modDate = dosDate();

    for (const auto& entry : entries) {
        uint32_t uncompressedSize = static_cast<uint32_t>(entry.data.size());
        uint32_t crc = 0;
        if (uncompressedSize > 0) {
            crc = crc32Calc(entry.data.data(), entry.data.size());
        }
        
        uint32_t compressedSize = 0;
        uint16_t compression = 0;
        std::vector<char> compressedData;

        if (uncompressedSize > 0) {
            z_stream strm = {};
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(entry.data.data()));
            strm.avail_in = uncompressedSize;
            uLongf destLen = compressBound(static_cast<uLong>(uncompressedSize));
            compressedData.resize(destLen);
            strm.next_out = reinterpret_cast<Bytef*>(compressedData.data());
            strm.avail_out = static_cast<uInt>(destLen);

            // -MAX_WBITS 产生 raw deflate 输出（无 zlib header），符合 ZIP 格式要求
            if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                            -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) == Z_OK) {
                int ret = deflate(&strm, Z_FINISH);
                deflateEnd(&strm);
                if (ret == Z_STREAM_END) {
                    compressedData.resize(strm.total_out);
                    compressedSize = static_cast<uint32_t>(strm.total_out);
                    compression = 8;  // deflate压缩
                } else {
                    // 压缩失败，使用存储模式（不压缩）
                    compressedData = entry.data;
                    compressedSize = uncompressedSize;
                    compression = 0;  // 存储模式
                }
            } else {
                // zlib初始化失败，使用存储模式
                compressedData = entry.data;
                compressedSize = uncompressedSize;
                compression = 0;  // 存储模式
            }
        } else {
            // 空文件
            compressedSize = 0;
            compression = 0;  // 存储模式
        }

        uint32_t localHeaderOffset = static_cast<uint32_t>(result.size());

        // 检查文件名是否包含非ASCII字符，决定是否使用UTF-8编码
        bool hasNonAscii = false;
        for (char c : entry.filename) {
            if (static_cast<unsigned char>(c) > 127) {
                hasNonAscii = true;
                break;
            }
        }
        
        // 通用位标志：第11位(0x0800)表示使用UTF-8编码
        uint16_t flags = hasNonAscii ? 0x0800 : 0;
        // 版本需要提取：20 = 2.0，支持UTF-8
        uint16_t versionNeeded = 20;

        writeU32LE(result, 0x04034b50);  // 本地文件头签名
        writeU16LE(result, versionNeeded);  // 版本需要提取
        writeU16LE(result, flags);  // 通用位标志
        writeU16LE(result, compression);
        writeU16LE(result, modTime);
        writeU16LE(result, modDate);
        writeU32LE(result, crc);
        writeU32LE(result, compressedSize);
        writeU32LE(result, uncompressedSize);
        writeU16LE(result, static_cast<uint16_t>(entry.filename.size()));
        writeU16LE(result, 0);  // 额外字段长度
        result.insert(result.end(), entry.filename.begin(), entry.filename.end());
        result.insert(result.end(), compressedData.begin(), compressedData.end());

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

    uint32_t centralDirOffset = static_cast<uint32_t>(result.size());

    for (const auto& cd : centralDir) {
        writeU32LE(result, 0x02014b50);
        writeU16LE(result, cd.versionMadeBy);
        writeU16LE(result, cd.versionNeeded);
        writeU16LE(result, cd.flags);
        writeU16LE(result, cd.compression);
        writeU16LE(result, cd.modTime);
        writeU16LE(result, cd.modDate);
        writeU32LE(result, cd.crc32);
        writeU32LE(result, cd.compressedSize);
        writeU32LE(result, cd.uncompressedSize);
        writeU16LE(result, cd.filenameLen);
        writeU16LE(result, cd.extraLen);
        writeU16LE(result, cd.commentLen);
        writeU16LE(result, cd.diskStart);
        writeU16LE(result, cd.internalAttr);
        writeU32LE(result, cd.externalAttr);
        writeU32LE(result, cd.localHeaderOffset);
        result.insert(result.end(), cd.filename.begin(), cd.filename.end());
    }

    uint32_t centralDirSize = static_cast<uint32_t>(result.size()) - centralDirOffset;

    writeU32LE(result, 0x06054b50);
    writeU16LE(result, 0);
    writeU16LE(result, 0);
    writeU16LE(result, static_cast<uint16_t>(centralDir.size()));
    writeU16LE(result, static_cast<uint16_t>(centralDir.size()));
    writeU32LE(result, centralDirSize);
    writeU32LE(result, centralDirOffset);
    writeU16LE(result, 0);

    return result;
}

} // namespace narnat
