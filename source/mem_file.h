#ifndef MEM_FILE_H
#define MEM_FILE_H

#include <string>
#include <vector>
#include <mutex>
#include <pthread.h>

enum MemBlockStatus
{
    MEM_BLOCK_STATUS_NOT_EXISTS,
    MEM_BLOCK_STATUS_CREATED,
    MEM_BLOCK_STATUS_DELETED
};

typedef struct
{
    size_t size;
    void* buf;
    bool is_last;
    MemBlockStatus status;
} MemBlock;

class MemFile
{
public:
    MemFile(const std::string& path, size_t block_size);
    ~MemFile();
    size_t Read(char* buf, size_t buf_size, size_t offset);
    size_t Write(char* buf, size_t buf_size);
    int Open();
    int Close();

private:
    std::vector<MemBlock*> mem_blocks;
    size_t write_offset;
    size_t block_size;
    int write_error;
    bool complete;
    MemBlock *block_in_progress;
    sem_t block_ready;

    MemBlock *NewBlock();
};

#endif