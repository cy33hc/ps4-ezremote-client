#include <stdio.h>
#include <stdio.h>
#include <string>

#include "common.h"
#include "split_file.h"

SplitFile::SplitFile(const std::string &path, size_t block_size)
{
    this->block_size = block_size;
    this->path = path;
    this->complete = false;
    sem_init(&this->block_ready, 0, 0);
}

SplitFile::~SplitFile()
{
    for (int i = 0; i < this->file_blocks.size(); i++)
    {
        if (this->file_blocks[i] != nullptr && this->file_blocks[i]->status != BLOCK_STATUS_DELETED)
        {
            if (this->file_blocks[i]->fd != nullptr)
            {
                fclose(this->file_blocks[i]->fd);
            }
            remove(this->file_blocks[i]->block_file.c_str());
            free(this->file_blocks[i]);
        }
    }
    sem_destroy(&this->block_ready);
};

int SplitFile::Open()
{
    this->block_in_progress = NewBlock();
    this->block_in_progress->fd = fopen(block_in_progress->block_file.c_str(), "w");

    return (block_in_progress->fd == nullptr);
}

size_t SplitFile::Read(char *buf, size_t buf_size, size_t offset)
{
    int first_block_num, block_num;
    size_t block_offset;
    size_t remaining;
    size_t bytes_read;
    size_t total_bytes_read;
    FileBlock *block;
    FILE *fd;
    char *p;

    first_block_num= offset / this->block_size;
    block_num = first_block_num;
    block_offset = offset % this->block_size;

    while ((block_num >= this->file_blocks.size() && !this->complete) ||
           (block_num < this->file_blocks.size() && this->file_blocks[block_num]->status == BLOCK_STATUS_NOT_EXISTS))
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        sem_timedwait(&this->block_ready, &ts);
    }

    block = this->file_blocks[block_num];
    if (block->status == BLOCK_STATUS_DELETED)
    {
        return -1;
    }

    if (block_offset > block->size - 1 && this->complete)
    {
        // requested offset is pass the end of split file
        return 0;
    }

    remaining = buf_size;
    bool eof = false;
    total_bytes_read = 0;
    p = buf;

    while (remaining > 0 && !eof)
    {
        fd = block->fd;
        if (fd == nullptr)
        {
            fd = fopen(block->block_file.c_str(), "rb");
            block->fd = fd;
        }

        fseek(fd, block_offset, SEEK_SET);
        bytes_read = fread(p, 1, remaining, fd);

        if (bytes_read == remaining)
        {
            p += bytes_read;
            total_bytes_read += bytes_read;
        }
        else
        {
            if (feof(fd))
            {
                p += bytes_read;
                total_bytes_read += bytes_read;
                if (block->is_last)
                {
                    eof = true;
                    continue;
                }
            }
            else
                return -1;
        }

        remaining -= bytes_read;

        if (remaining == 0)
            continue;

        block_num++;
        block_offset = 0;

        while ((block_num > this->file_blocks.size() - 1 && !this->complete) ||
               this->file_blocks[block_num]->status == BLOCK_STATUS_NOT_EXISTS)
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            sem_timedwait(&this->block_ready, &ts);
        }

        block = this->file_blocks[block_num];
    }

    // delete blocks before the first read offset block. Assumuption, that reads are always
    // forward and won't read previously already read blocks. For safety, keeping only current block and 2 previous blocks
    for (int j=0; j < first_block_num - 13; j++)
    {
        if (this->file_blocks[j]->status == BLOCK_STATUS_CREATED)
        {
            if (this->file_blocks[j]->fd != nullptr)
            {
                fclose(this->file_blocks[j]->fd);
                this->file_blocks[j]->fd = nullptr;
            }
            this->file_blocks[j]->status = BLOCK_STATUS_DELETED;
            remove(this->file_blocks[j]->block_file.c_str());
        }
    }

    this->read_offset = offset + total_bytes_read;
    return total_bytes_read;
}

size_t SplitFile::Write(char *buf, size_t buf_size)
{
    size_t bytes_written = 0;
    size_t block_space_remaining;
    size_t bytes_to_write;

    char *p = buf;
    size_t total_bytes_written = 0;
    size_t remaining_to_write = buf_size;

    if (this->IsClosed())
        return -1;

    while (remaining_to_write > 0 && !this->complete)
    {
        block_space_remaining = this->block_size - block_in_progress->size;
        bytes_to_write = MIN(remaining_to_write, block_space_remaining);

        bytes_written = fwrite(p, 1, bytes_to_write, block_in_progress->fd);
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
            fflush(block_in_progress->fd);
            fclose(block_in_progress->fd);
            block_in_progress->fd = nullptr;
            block_in_progress->status = BLOCK_STATUS_CREATED;
            this->file_blocks.push_back(block_in_progress);

            sem_post(&this->block_ready);

            block_in_progress = NewBlock();
        }
    }
    this->write_offset += total_bytes_written;

    return total_bytes_written;
}

int SplitFile::Close()
{
    if (this->complete)
        return 0;

    this->complete = true;

    if (block_in_progress->fd != nullptr)
    {
        fflush(block_in_progress->fd);
        fclose(block_in_progress->fd);
        block_in_progress->fd = nullptr;
    }
    block_in_progress->status = BLOCK_STATUS_CREATED;
    block_in_progress->is_last = true;
    this->file_blocks.push_back(block_in_progress);
    sem_post(&this->block_ready);

    // Wait until file is fully read, if file isn't full read
    // in 5 mins then go ahead and delete all file chunks
    int retries = 10;
    size_t prev_read_offset = 0;
    while (this->read_offset != this->write_offset && retries > 0)
    {
        if (prev_read_offset == this->read_offset)
            retries--;
        prev_read_offset = this->read_offset;
        sceKernelUsleep(1000000);
    }
    sceKernelUsleep(5000000);

    for (size_t j = 0; j < this->file_blocks.size(); j++)
    {
        if (this->file_blocks[j] != nullptr && this->file_blocks[j]->status == BLOCK_STATUS_CREATED)
        {
            remove(this->file_blocks[j]->block_file.c_str());
        }
    }
    return 0;
}

bool SplitFile::IsClosed()
{
    return this->complete;
}

FileBlock *SplitFile::NewBlock()
{
    FileBlock *block = (FileBlock *)malloc(sizeof(FileBlock));
    memset(block, 0, sizeof(FileBlock));

    block->is_last = false;
    block->size = 0;
    block->block_file = this->path + "." + std::to_string(this->file_blocks.size());
    block->fd = fopen(block->block_file.c_str(), "w");

    return block;
}
