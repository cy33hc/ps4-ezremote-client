#ifndef ZIP_UTIL_H
#define ZIP_UTIL_H

#include <string.h>
#include <stdlib.h>
#include <minizip/unzip.h>
#include <minizip/zip.h>
#include <archive.h>
#include <archive_entry.h>
#include "common.h"
#include "fs.h"

#define ARCHIVE_TRANSFER_SIZE (512 * 1024)

static uint8_t MAGIC_ZIP_1[4] = {0x50, 0x4B, 0x03, 0x04};
static uint8_t MAGIC_ZIP_2[4] = {0x50, 0x4B, 0x05, 0x06};
static uint8_t MAGIC_ZIP_3[4] = {0x50, 0x4B, 0x07, 0x08};

static uint8_t MAGIC_7Z_1[6] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C};

static uint8_t MAGIC_RAR_1[7] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00};
static uint8_t MAGIC_RAR_2[8] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00};

enum CompressFileType {
    COMPRESS_FILE_TYPE_7Z,
    COMPRESS_FILE_TYPE_ZIP,
    COMPRESS_FILE_TYPE_RAR,
    COMPRESS_FILE_TYPE_UNKNOWN
};

struct RemoteArchiveData
{
    std::string path;
    ssize_t size;
    ssize_t offset;
    uint8_t buf[ARCHIVE_TRANSFER_SIZE];
    RemoteClient *client;
};

struct ArchiveEntry
{
    struct archive *archive;
    struct archive_entry *entry;
    std::string filename;
    size_t filesize;
    RemoteArchiveData *client_data;
};

namespace ZipUtil
{
    int ZipAddPath(zipFile zf, const std::string &path, int filename_start, int level);
    int Extract(const DirEntry &file, const std::string &dir, bool is_remote = false);
    ArchiveEntry *GetPackageEntry(const std::string &zip_file, bool is_remote = false);
}
#endif