#ifndef EZ_SPLIT_FILE_H
#define EZ_SPLIT_FILE_H

#include <string>
#include <vector>
#include <mutex>
#include <pthread.h>

enum FileBlockStatus
{
    BLOCK_STATUS_NOT_EXISTS,
    BLOCK_STATUS_CREATED,
    BLOCK_STATUS_DELETED
};

typedef struct
{
    std::string block_file;
    size_t size;
    FILE* fd;
    bool is_last;
    FileBlockStatus status;
} FileBlock;

class SplitFile
{
public:
    SplitFile(const std::string& path, size_t block_size);
    ~SplitFile();
    size_t Read(char* buf, size_t buf_size, size_t offset);
    size_t Write(char* buf, size_t buf_size);
    int Open();
    int Close();
    bool IsClosed();

private:
    std::vector<FileBlock*> file_blocks;
    size_t write_offset;
    size_t block_size;
    std::string path;
    int write_error;
    bool complete;
    FileBlock *block_in_progress;
    sem_t block_ready;

    FileBlock *NewBlock();
};

#endif