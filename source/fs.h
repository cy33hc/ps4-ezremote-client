#ifndef LAUNCHER_FS_H
#define LAUNCHER_FS_H

#pragma once
#include <string.h>
#include <string>
#include <vector>
#include <cstdint>

#include "common.h"

#define MAX_PATH_LENGTH 1024

namespace FS
{
    std::string GetPath(const std::string &path1, const std::string &path2);

    void MkDirs(const std::string &path, bool prev = false);

    void Rm(const std::string &file);
    void RmDir(const std::string &path);
    int RmRecursive(const std::string &path);

    int64_t GetSize(const std::string &path);

    bool FileExists(const std::string &path);
    bool FolderExists(const std::string &path);

    void Rename(const std::string &from, const std::string &to);

    // creates file (if it exists, truncates size to 0)
    FILE *Create(const std::string &path);

    // open existing file in read/write, fails if file does not exist
    FILE *OpenRW(const std::string &path);

    // open existing file in read/write, fails if file does not exist
    FILE *OpenRead(const std::string &path);

    // open file for writing, next write will append data to end of it
    FILE *Append(const std::string &path);

    void Close(FILE *f);

    int64_t Seek(FILE *f, uint64_t offset);
    int Read(FILE *f, void *buffer, uint32_t size);
    int Write(FILE *f, const void *buffer, uint32_t size);

    std::vector<char> Load(const std::string &path);
    void Save(const std::string &path, const void *data, uint32_t size);

    std::vector<std::string> ListFiles(const std::string &path);
    std::vector<DirEntry> ListDir(const std::string &path, int *err);

    void Sort(std::vector<DirEntry> &list);

    int hasEndSlash(const char *path);

    int Head(const std::string &path, void* buffer, uint16_t len);
}

#endif