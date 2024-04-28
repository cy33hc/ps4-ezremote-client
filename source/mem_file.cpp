#include <stdio.h>
#include <stdio.h>
#include <string>

#include "common.h"
#include "mem_file.h"

MemFile::MemFile(const std::string &path, size_t block_size)
{
    this->block_size = block_size;
    this->complete = false;
    sem_init(&this->block_ready, 0, 0);
}

MemFile::~MemFile()
{
    for (int i = 0; i < this->mem_blocks.size(); i++)
    {
        if (this->mem_blocks[i] != nullptr && this->mem_blocks[i]->status != MEM_BLOCK_STATUS_DELETED)
        {
            if (this->mem_blocks[i]->buf != nullptr)
            {
                free(this->mem_blocks[i]->buf);
                this->mem_blocks[i]->buf = nullptr;
            }
            free(this->mem_blocks[i]);
        }
    }
    sem_destroy(&this->block_ready);
};

int MemFile::Open()
{
    this->block_in_progress = NewBlock();
    this->block_in_progress->buf = malloc(block_size);

    return (block_in_progress->buf == nullptr);
}

size_t MemFile::Read(char *buf, size_t buf_size, size_t offset)
{
    int first_block_num, block_num;
    size_t block_offset;
    size_t remaining;
    size_t bytes_read;
    size_t total_bytes_read;
    MemBlock *block;
    char *p;

    first_block_num= offset / this->block_size;
    block_num = first_block_num;
    block_offset = offset % this->block_size;

    while ((block_num >= this->mem_blocks.size() && !this->complete) ||
           (block_num < this->mem_blocks.size() && this->mem_blocks[block_num]->status == MEM_BLOCK_STATUS_NOT_EXISTS))
    {
        sem_wait(&this->block_ready);
    }

    block = this->mem_blocks[block_num];
    if (block->status == MEM_BLOCK_STATUS_DELETED)
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
        uint8_t *src = (uint8_t*)block->buf;
        src += block_offset;
        bytes_read = block_size - block_offset;
        memcpy(p, src, bytes_read);

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
               this->mem_blocks[block_num]->status == MEM_BLOCK_STATUS_NOT_EXISTS)
        {
            sem_wait(&this->block_ready);
        }

        block = this->mem_blocks[block_num];
    }

    // delete blocks before the first read offset block. Assumuption, that reads are always
    // forward and won't read previously already read blocks. For safety, keeping only current block and 2 previous blocks
    for (int j=0; j < first_block_num - 2; j++)
    {
        if (this->mem_blocks[j]->status == MEM_BLOCK_STATUS_CREATED)
        {
            if (this->mem_blocks[j]->buf != nullptr)
            {
                free(this->mem_blocks[j]->buf);
                this->mem_blocks[j]->buf = nullptr;
            }
            this->mem_blocks[j]->status = MEM_BLOCK_STATUS_DELETED;
        }
    }

    return total_bytes_read;
}

size_t MemFile::Write(char *buf, size_t buf_size)
{
    size_t bytes_written;
    size_t block_space_remaining;
    size_t bytes_to_write;

    char *p = buf;
    size_t total_bytes_written = 0;
    size_t remaining_to_write = buf_size;

    while (remaining_to_write > 0)
    {
        block_space_remaining = this->block_size - block_in_progress->size;
        bytes_to_write = MIN(remaining_to_write, block_space_remaining);
        memcpy(block_in_progress->buf, p, bytes_to_write);
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

    return total_bytes_written;
}

int MemFile::Close()
{
    block_in_progress->status = MEM_BLOCK_STATUS_CREATED;
    block_in_progress->is_last = true;
    this->mem_blocks.push_back(block_in_progress);
    this->complete = true;
    sem_post(&this->block_ready);

    return 0;
}

MemBlock *MemFile::NewBlock()
{
    MemBlock *block = (MemBlock *)malloc(sizeof(MemBlock));
    memset(block, 0, sizeof(MemBlock));

    block->is_last = false;
    block->size = 0;
    block->buf = malloc(block_size);

    return block;
}