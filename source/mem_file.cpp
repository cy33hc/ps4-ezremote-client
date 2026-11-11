#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unistd.h"
#include <time.h>
#include <string>

#include "common.h"
#include "mem_file.h"

static const double MEM_FILE_READ_SPEED_WINDOW_SECS = 1.0;
static const double MEM_FILE_WRITE_MAX_SLEEP_SECS = 0.1;
static const long MEM_FILE_WRITE_STARTUP_WAIT_NSEC = 10 * 1000 * 1000; // 10ms

static double TimespecDiffSecs(const struct timespec &start, const struct timespec &end)
{
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

MemFile::MemFile(size_t block_size, size_t write_throttle_distance, double write_speedup)
{
    this->block_size = block_size;
    this->complete = false;
    this->write_throttle_distance = write_throttle_distance;
    this->write_speedup = write_speedup;
    sem_init(&this->block_ready, 0, 0);
}

MemFile::~MemFile()
{
    for (int i = 0; i < this->mem_blocks.size(); i++)
    {
        if (this->mem_blocks[i] != nullptr && this->mem_blocks[i]->status != MEM_BLOCK_STATUS_DELETED)
        {
            if (this->mem_blocks[i]->data != nullptr)
            {
                free(this->mem_blocks[i]->data);
            }
            delete this->mem_blocks[i];
        }
    }
    sem_destroy(&this->block_ready);
};

int MemFile::Open()
{
    this->block_in_progress = NewBlock();
    return (block_in_progress->data == nullptr);
}

ssize_t MemFile::Read(char *buf, size_t buf_size, size_t offset)
{
    int first_block_num, block_num;
    size_t block_offset;
    size_t remaining;
    size_t bytes_read;
    ssize_t total_bytes_read;
    MemBlock *block;
    char *p;

    first_block_num= offset / this->block_size;
    block_num = first_block_num;
    block_offset = offset % this->block_size;

    while ((block_num >= this->mem_blocks.size() && !this->complete) ||
           (block_num < this->mem_blocks.size() && this->mem_blocks[block_num]->status == MEM_BLOCK_STATUS_NOT_EXISTS))
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        sem_timedwait(&this->block_ready, &ts);
    }

    // If complete and block_num is past the end, the requested offset is beyond EOF
    if (block_num >= this->mem_blocks.size())
        return 0;

    block = this->mem_blocks[block_num];
    if (block->status == MEM_BLOCK_STATUS_DELETED)
    {
        return -1;
    }

    if (block_offset > block->size - 1 && this->complete)
    {
        // requested offset is pass the end of mem file
        return 0;
    }

    remaining = buf_size;
    bool eof = false;
    total_bytes_read = 0;
    p = buf;

    while (remaining > 0 && !eof)
    {
        if (block->data == nullptr || block_offset >= block->size)
        {
            bytes_read = 0;
        }
        else
        {
            bytes_read = MIN(remaining, block->size - block_offset);
            memcpy(p, block->data + block_offset, bytes_read);
        }

        if (bytes_read == remaining)
        {
            p += bytes_read;
            total_bytes_read += bytes_read;
        }
        else
        {
            p += bytes_read;
            total_bytes_read += bytes_read;
            if (block->is_last)
            {
                eof = true;
                continue;
            }
        }

        remaining -= bytes_read;

        if (remaining == 0)
            continue;

        block_num++;
        block_offset = 0;

        while ((block_num > this->mem_blocks.size() - 1 && !this->complete) ||
               (block_num < this->mem_blocks.size() && this->mem_blocks[block_num]->status == MEM_BLOCK_STATUS_NOT_EXISTS))
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            sem_timedwait(&this->block_ready, &ts);
        }

        // If complete and block_num is past the end, no more data
        if (block_num >= this->mem_blocks.size())
            break;

        block = this->mem_blocks[block_num];
    }

    // delete blocks before the first read offset block. Assumuption, that reads are always
    // forward and won't read previously already read blocks. For safety, keeping only current block and 2 previous blocks
    for (int j=0; j < first_block_num - 7; j++)
    {
        if (this->mem_blocks[j] != nullptr && this->mem_blocks[j]->status == MEM_BLOCK_STATUS_CREATED)
        {
            if (this->mem_blocks[j]->data != nullptr)
            {
                free(this->mem_blocks[j]->data);
                this->mem_blocks[j]->data = nullptr;
            }
            this->mem_blocks[j]->status = MEM_BLOCK_STATUS_DELETED;
            delete (this->mem_blocks[j]);
            this->mem_blocks[j] = nullptr;
        }
    }

    this->read_offset = offset + total_bytes_read;
    this->read_bytes_total += total_bytes_read;
    return total_bytes_read;
}

double MemFile::GetReadSpeed()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    size_t total = this->read_bytes_total;

    if (!this->read_speed_sampling)
    {
        // first call: start sampling, no rate yet
        this->read_speed_sampling = true;
        this->read_speed_sample_time = now;
        this->read_speed_sample_bytes = total;
        return 0.0;
    }

    double elapsed = TimespecDiffSecs(this->read_speed_sample_time, now);
    if (elapsed < MEM_FILE_READ_SPEED_WINDOW_SECS)
        return this->read_speed;

    size_t bytes = (total >= this->read_speed_sample_bytes) ? total - this->read_speed_sample_bytes : 0;

    this->read_speed = bytes / elapsed;
    this->read_speed_sample_bytes = total;
    this->read_speed_sample_time = now;

    return this->read_speed;
}

size_t MemFile::GetReadBytes()
{
    return this->read_bytes_total;
}

void MemFile::SetWriteSpeedup(double write_speedup)
{
    this->write_speedup = write_speedup;
}

void MemFile::ThrottleWrite(size_t pending_write_offset)
{
    if (this->write_throttle_distance == 0)
        return;

    for (;;)
    {
        // Close() was called: the reader is gone, do not block the writer for ever
        if (this->complete)
            return;

        size_t ahead = (pending_write_offset > this->read_offset) ? pending_write_offset - this->read_offset : 0;
        if (ahead <= this->write_throttle_distance)
        {
            // back within the allowed distance: start a new measurement
            this->write_throttle_sampling = false;
            return;
        }

        double read_speed = this->GetReadSpeed();
        if (read_speed <= 0.0)
        {
            // The reader has not started yet, wait for it instead of racing ahead
            struct timespec nap = {0, MEM_FILE_WRITE_STARTUP_WAIT_NSEC};
            nanosleep(&nap, NULL);
            continue;
        }

        // Bytes per second the writer is allowed to do. A non-positive multiplier
        // disables the pacing.
        double speedup = this->write_speedup;
        if (speedup <= 0.0)
            return;

        double target = speedup * read_speed;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (!this->write_throttle_sampling)
        {
            // first time over the distance: start measuring from here
            this->write_throttle_sampling = true;
            this->write_throttle_sample_time = now;
            this->write_throttle_sample_offset = pending_write_offset;
            return;
        }

        double elapsed = TimespecDiffSecs(this->write_throttle_sample_time, now);
        double written = (double)(pending_write_offset - this->write_throttle_sample_offset);
        double allowed = target * elapsed;

        if (written <= allowed)
            return;

        // Over budget: sleep for the time we are ahead by, then measure again
        double wait = (written - allowed) / target;
        if (wait > MEM_FILE_WRITE_MAX_SLEEP_SECS)
            wait = MEM_FILE_WRITE_MAX_SLEEP_SECS;

        struct timespec nap;
        nap.tv_sec = (time_t)wait;
        nap.tv_nsec = (long)((wait - (double)nap.tv_sec) * 1000000000.0);
        nanosleep(&nap, NULL);

        clock_gettime(CLOCK_MONOTONIC, &this->write_throttle_sample_time);
        this->write_throttle_sample_offset = pending_write_offset;
    }
}

ssize_t MemFile::Write(char *buf, size_t buf_size)
{
    size_t bytes_written = 0;
    size_t block_space_remaining;
    size_t bytes_to_write;

    char *p = buf;
    ssize_t total_bytes_written = 0;
    size_t remaining_to_write = buf_size;

    if (this->IsClosed())
        return -1;

    while (remaining_to_write > 0 && !this->complete)
    {
        // Do not let the writer run away from the reader
        this->ThrottleWrite(this->write_offset + total_bytes_written);

        block_space_remaining = this->block_size - block_in_progress->size;
        bytes_to_write = MIN(remaining_to_write, block_space_remaining);

        if (block_in_progress->data != nullptr)
            memcpy(block_in_progress->data + block_in_progress->size, p, bytes_to_write);
        bytes_written = bytes_to_write;
        block_in_progress->size += bytes_written;
        total_bytes_written += bytes_written;
        remaining_to_write -= bytes_written;
        block_space_remaining -= bytes_written;
        p += bytes_written;

        // error if bytes_to_write != bytes_written
        if (bytes_written != bytes_to_write)
        {
            break;
        }

        if (block_space_remaining == 0)
        {
            block_in_progress->status = MEM_BLOCK_STATUS_CREATED;
            this->mem_blocks.push_back(block_in_progress);

            sem_post(&this->block_ready);

            block_in_progress = NewBlock();
        }
    }
    this->write_offset += total_bytes_written;

    return total_bytes_written;
}

int MemFile::Close()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (this->complete)
            return 0;

        this->complete = true;

        block_in_progress->status = MEM_BLOCK_STATUS_CREATED;
        block_in_progress->is_last = true;
        this->mem_blocks.push_back(block_in_progress);
        sem_post(&this->block_ready);
    } // lock released here — readers can proceed while we wait

    // Wait until file is fully read, if file isn't full read
    // in 5 mins then go ahead and delete all file chunks
    int retries = 10;
    size_t prev_read_offset = 0;
    while (this->read_offset != this->write_offset && retries > 0)
    {
        if (prev_read_offset == this->read_offset)
            retries--;
        prev_read_offset = this->read_offset;
        sleep(1);
    }
    sleep(5);

    for (size_t j = 0; j < this->mem_blocks.size(); j++)
    {
        if (this->mem_blocks[j] != nullptr && this->mem_blocks[j]->status == MEM_BLOCK_STATUS_CREATED)
        {
            if (this->mem_blocks[j]->data != nullptr)
            {
                free(this->mem_blocks[j]->data);
                this->mem_blocks[j]->data = nullptr;
            }
        }
    }
    return 0;
}

bool MemFile::IsClosed()
{
    return this->complete;
}

MemBlock *MemFile::NewBlock()
{
    MemBlock *block = new MemBlock{};

    block->is_last = false;
    block->size = 0;
    block->data = (char *)malloc(this->block_size);

    return block;
}
