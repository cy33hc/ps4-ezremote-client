#ifndef EZ_SPLIT_FILE_H
#define EZ_SPLIT_FILE_H

#include <string>
#include <vector>
#include <mutex>
#include <time.h>
#include <semaphore.h>
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
    // write_throttle_distance: once the writer is more than this many bytes ahead
    // of read_offset, Write() is paced to 2x the measured read speed. 0 disables
    // the throttle (default), a sane value is a couple of blocks.
    SplitFile(const std::string& path, size_t block_size, size_t write_throttle_distance = 0);
    ~SplitFile();
    size_t Read(char* buf, size_t buf_size, size_t offset);
    ssize_t Write(char* buf, size_t buf_size);
    int Open();
    int Close();
    bool IsClosed();

    // Approximate read throughput. Read() only adds the bytes it returned to a
    // counter, everything else happens when one of these is called. Kept
    // lock-free on purpose: it is a rough estimate for monitoring, so a lost
    // update now and then does not matter and the read path stays untouched.
    double GetReadSpeed();   // bytes/s, rate of the time since the last sample
    size_t GetReadBytes();   // total bytes returned by Read()

private:
    std::vector<FileBlock*> file_blocks;
    size_t write_offset = 0;
    size_t block_size;
    size_t read_offset = 0;
    std::string path;
    int write_error;
    bool complete;
    FileBlock *block_in_progress;
    sem_t block_ready;

    // Read speed estimate: read_bytes_total is bumped by Read(), the sampling
    // state is only touched by GetReadSpeed().
    size_t read_bytes_total = 0;
    bool read_speed_sampling = false;
    size_t read_speed_sample_bytes = 0;
    double read_speed = 0.0;
    struct timespec read_speed_sample_time = {0, 0};

    // Write throttle: inert while the writer is within write_throttle_distance of
    // the reader, afterwards Write() is paced to 2x the read speed.
    size_t write_throttle_distance = 0;
    bool write_throttle_sampling = false;
    size_t write_throttle_sample_offset = 0;
    struct timespec write_throttle_sample_time = {0, 0};

    void ThrottleWrite(size_t pending_write_offset);
    FileBlock *NewBlock();
};

#endif
