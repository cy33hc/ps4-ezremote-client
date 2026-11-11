#ifndef EZ_MEM_FILE_H
#define EZ_MEM_FILE_H

#include <string>
#include <deque>
#include <mutex>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <mutex>

enum MemBlockStatus
{
    MEM_BLOCK_STATUS_NOT_EXISTS,
    MEM_BLOCK_STATUS_CREATED,
    MEM_BLOCK_STATUS_DELETED
};

typedef struct
{
    size_t size;
    char* data;
    bool is_last;
    MemBlockStatus status;
} MemBlock;

class MemFile
{
public:
    // The writer is paced to the measured read speed, so 1.0 means "no faster
    // than the reader".
    static constexpr double DEFAULT_WRITE_SPEEDUP = 1.0;
    // Only throttle once the writer is this far ahead of the reader.
    static constexpr size_t DEFAULT_WRITE_THROTTLE_DISTANCE = 32 * 1024 * 1024;

    MemFile(size_t block_size, size_t write_throttle_distance = DEFAULT_WRITE_THROTTLE_DISTANCE,
            double write_speedup = DEFAULT_WRITE_SPEEDUP);
    ~MemFile();
    ssize_t Read(char* buf, size_t buf_size, size_t offset);
    ssize_t Write(char* buf, size_t buf_size);
    int Open();
    int Close();
    bool IsClosed();

    void SetWriteSpeedup(double write_speedup);
    double GetWriteSpeedup() const { return this->write_speedup; }

    double GetReadSpeed();   // bytes/s, rate of the time since the last sample
    size_t GetReadBytes();   // total bytes returned by Read()

private:
    std::deque<MemBlock*> mem_blocks;
    size_t write_offset = 0;
    size_t block_size;
    size_t read_offset = 0;
    int write_error;
    bool complete;
    MemBlock *block_in_progress;
    sem_t block_ready;
    std::mutex mutex_;

    size_t read_bytes_total = 0;
    bool read_speed_sampling = false;
    size_t read_speed_sample_bytes = 0;
    double read_speed = 0.0;
    struct timespec read_speed_sample_time = {0, 0};

    size_t write_throttle_distance = 0;
    double write_speedup = DEFAULT_WRITE_SPEEDUP;
    bool write_throttle_sampling = false;
    size_t write_throttle_sample_offset = 0;
    struct timespec write_throttle_sample_time = {0, 0};

    void ThrottleWrite(size_t pending_write_offset);
    MemBlock *NewBlock();
};

#endif
