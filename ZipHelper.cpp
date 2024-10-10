#include "ZipHelper.h"
#include <stdexcept>

void addFileToZip(zip_t* zip, const std::string& path, const std::string& zipPath) {
    zip_source_t* source = zip_source_file(zip, path.c_str(), 0, 0);
    if (source == nullptr || zip_file_add(zip, zipPath.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
        zip_source_free(source);
        throw std::runtime_error("Error adding file to zip: " + path);
    }
}
