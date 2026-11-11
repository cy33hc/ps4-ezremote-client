#ifndef EZ_FILE_STREAM_H
#define EZ_FILE_STREAM_H

#include <string>

#include "split_file.h"
#include "mem_file.h"

// Which block store the download/install pipeline uses.
//
//   0 = SplitFile (blocks in temp files on disk)
//   1 = MemFile   (blocks in heap buffers)
//
// Overridable from the build: -DFILE_STREAM_USE_MEMORY=1
#ifndef FILE_STREAM_USE_MEMORY
#define FILE_STREAM_USE_MEMORY 0
#endif

#if FILE_STREAM_USE_MEMORY
typedef MemFile FileStream;
#else
typedef SplitFile FileStream;
#endif

// path is ignored when MemFile is selected.
FileStream *CreateFileStream(const std::string &path, size_t block_size);

#endif
