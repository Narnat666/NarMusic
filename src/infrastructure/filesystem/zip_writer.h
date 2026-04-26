#ifndef NARNAT_ZIP_WRITER_H
#define NARNAT_ZIP_WRITER_H

#include <string>
#include <vector>

namespace narnat {

struct ZipEntry {
    std::string filename;
    std::vector<char> data;
};

class ZipWriter {
public:
    static std::vector<char> create(const std::vector<ZipEntry>& entries);
};

} // namespace narnat

#endif
