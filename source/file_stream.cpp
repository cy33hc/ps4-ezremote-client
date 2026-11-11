#include "file_stream.h"
#include "installer.h"

FileStream *CreateFileStream(const std::string &path, size_t block_size)
{
#if FILE_STREAM_USE_MEMORY
    // MemFile has no file to write, and applies its own pacing defaults
    // (write at read speed, throttle past 32MB).
    (void)path;
    return new MemFile(block_size);
#else
    return new SplitFile(path, block_size, INSTALL_ARCHIVE_PKG_WRITE_THROTTLE_DISTANCE);
#endif
}
