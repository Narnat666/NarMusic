#ifndef NARNAT_ZIP_WRITER_H
#define NARNAT_ZIP_WRITER_H

#include <string>
#include <vector>

namespace narnat {

struct ZipEntry {
    std::string filename;
    std::string filePath;
};

class ZipWriter {
public:
    static bool createToFile(const std::vector<ZipEntry>& entries, const std::string& outputPath);
};

} // namespace narnat

#endif
