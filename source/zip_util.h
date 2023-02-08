#ifndef ZIP_UTIL_H
#define ZIP_UTIL_H

#include <string.h>
#include <stdlib.h>
#include <minizip/unzip.h>
#include <minizip/zip.h>
#include "common.h"
#include "fs.h"

namespace ZipUtil
{
    int ZipAddPath(zipFile zf, const std::string &path, int filename_start, int level);
    int Extract(const DirEntry &file, const std::string &dir);
}
#endif