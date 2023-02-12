
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <minizip/unzip.h>
#include <minizip/zip.h>
#include <un7zip.h>
#include <unrar.h>
#include "common.h"
#include "fs.h"
#include "lang.h"
#include "rtc.h"
#include "windows.h"
#include "zip_util.h"

#define TRANSFER_SIZE (128 * 1024)

namespace ZipUtil
{
    static char filename_extracted[256];

    void callback_7zip(const char *fileName, unsigned long fileSize, unsigned fileNum, unsigned numFiles)
    {
        sprintf(activity_message, "%s %s: %s", lang_strings[STR_EXTRACTING], filename_extracted, fileName);
    }

    void convertToZipTime(time_t time, tm_zip *tmzip)
    {
        OrbisDateTime gmt;
        OrbisDateTime lt;

        struct tm tm = *localtime(&time);
        gmt.day = tm.tm_mday;
        gmt.month = tm.tm_mon + 1;
        gmt.year = tm.tm_year + 1900;
        gmt.hour = tm.tm_hour;
        gmt.minute = tm.tm_min;
        gmt.second = tm.tm_sec;

        convertUtcToLocalTime(&gmt, &lt);

        tmzip->tm_sec = lt.second;
        tmzip->tm_min = lt.minute;
        tmzip->tm_hour = lt.hour;
        tmzip->tm_mday = lt.day;
        tmzip->tm_mon = lt.month;
        tmzip->tm_year = lt.year;
    }

    int ZipAddFile(zipFile zf, const std::string &path, int filename_start, int level)
    {
        int res;
        // Get file stat
        struct stat file_stat;
        memset(&file_stat, 0, sizeof(file_stat));
        res = stat(path.c_str(), &file_stat);
        if (res < 0)
            return res;

        // Get file local time
        zip_fileinfo zi;
        memset(&zi, 0, sizeof(zip_fileinfo));
        convertToZipTime(file_stat.st_mtim.tv_sec, &zi.tmz_date);

        bytes_transfered = 0;
        bytes_to_download = file_stat.st_size;

        // Large file?
        int use_zip64 = (file_stat.st_size >= 0xFFFFFFFF);

        // Open new file in zip
        res = zipOpenNewFileInZip3_64(zf, path.substr(filename_start).c_str(), &zi,
                                      NULL, 0, NULL, 0, NULL,
                                      (level != 0) ? Z_DEFLATED : 0,
                                      level, 0,
                                      -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                      NULL, 0, use_zip64);
        if (res < 0)
            return res;

        // Open file to add
        FILE *fd = FS::OpenRead(path);
        if (fd == NULL)
        {
            zipCloseFileInZip(zf);
            return 0;
        }

        // Add file to zip
        void *buf = memalign(4096, TRANSFER_SIZE);
        uint64_t seek = 0;

        while (1)
        {
            int read = FS::Read(fd, buf, TRANSFER_SIZE);
            if (read < 0)
            {
                free(buf);
                FS::Close(fd);
                zipCloseFileInZip(zf);
                return read;
            }

            if (read == 0)
                break;

            int written = zipWriteInFileInZip(zf, buf, read);
            if (written < 0)
            {
                free(buf);
                FS::Close(fd);
                zipCloseFileInZip(zf);
                return written;
            }

            seek += written;
            bytes_transfered += read;
        }

        free(buf);
        FS::Close(fd);
        zipCloseFileInZip(zf);

        return 1;
    }

    int ZipAddFolder(zipFile zf, const std::string &path, int filename_start, int level)
    {
        int res;

        // Get file stat
        struct stat file_stat;
        memset(&file_stat, 0, sizeof(file_stat));
        res = stat(path.c_str(), &file_stat);
        if (res < 0)
            return res;

        // Get file local time
        zip_fileinfo zi;
        memset(&zi, 0, sizeof(zip_fileinfo));
        convertToZipTime(file_stat.st_mtim.tv_sec, &zi.tmz_date);

        // Open new file in zip
        std::string folder = path.substr(filename_start);
        if (folder[folder.length() - 1] != '/')
            folder = folder + "/";

        res = zipOpenNewFileInZip3_64(zf, folder.c_str(), &zi,
                                      NULL, 0, NULL, 0, NULL,
                                      (level != 0) ? Z_DEFLATED : 0,
                                      level, 0,
                                      -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                      NULL, 0, 0);

        if (res < 0)
            return res;

        zipCloseFileInZip(zf);
        return 1;
    }

    int ZipAddPath(zipFile zf, const std::string &path, int filename_start, int level)
    {
        DIR *dfd = opendir(path.c_str());
        if (dfd != NULL)
        {
            int ret = ZipAddFolder(zf, path, filename_start, level);
            if (ret <= 0)
                return ret;

            struct dirent *dirent;
            do
            {
                dirent = readdir(dfd);
                if (stop_activity)
                    return 1;
                if (dirent != NULL && strcmp(dirent->d_name, ".") != 0 && strcmp(dirent->d_name, "..") != 0)
                {
                    int new_path_length = path.length() + strlen(dirent->d_name) + 2;
                    char *new_path = (char *)malloc(new_path_length);
                    snprintf(new_path, new_path_length, "%s%s%s", path.c_str(), FS::hasEndSlash(path.c_str()) ? "" : "/", dirent->d_name);

                    int ret = 0;

                    if (dirent->d_type & DT_DIR)
                    {
                        ret = ZipAddPath(zf, new_path, filename_start, level);
                    }
                    else
                    {
                        sprintf(activity_message, "%s %s", lang_strings[STR_COMPRESSING], new_path);
                        ret = ZipAddFile(zf, new_path, filename_start, level);
                    }

                    free(new_path);

                    // Some folders are protected and return 0x80010001. Bypass them
                    if (ret <= 0)
                    {
                        closedir(dfd);
                        return ret;
                    }
                }
            } while (dirent != NULL);

            closedir(dfd);
        }
        else
        {
            return ZipAddFile(zf, path, filename_start, level);
        }

        return 1;
    }

    CompressFileType getCompressFileType(const std::string &file)
    {
        char buf[8];

        memset(buf, 0, 8);
        int ret = FS::Head(file, buf, 8);
        if (ret == 0)
            return COMPRESS_FILE_TYPE_UNKNOWN;

        if (strncmp(buf, (const char *)MAGIC_7Z_1, 6) == 0)
            return COMPRESS_FILE_TYPE_7Z;
        else if (strncmp(buf, (const char *)MAGIC_RAR_1, 7) == 0 || strncmp(buf, (const char *)MAGIC_RAR_2, 8) == 0)
            return COMPRESS_FILE_TYPE_RAR;
        else if (strncmp(buf, (const char *)MAGIC_ZIP_1, 4) == 0 || strncmp(buf, (const char *)MAGIC_ZIP_2, 4) == 0 || strncmp(buf, (const char *)MAGIC_ZIP_3, 4) == 0)
            return COMPRESS_FILE_TYPE_ZIP;

        return COMPRESS_FILE_TYPE_UNKNOWN;
    }

    int ExtractZip(const DirEntry &file, const std::string &dir)
    {
        file_transfering = true;
        unz_global_info global_info;
        unz_file_info file_info;
        unzFile zipfile = unzOpen(file.path);
        std::string dest_dir = std::string(dir);
        if (dest_dir[dest_dir.length() - 1] != '/')
        {
            dest_dir = dest_dir + "/";
        }
        if (zipfile == NULL)
        {
            return 0;
        }
        unzGetGlobalInfo(zipfile, &global_info);
        unzGoToFirstFile(zipfile);
        uint64_t curr_extracted_bytes = 0;
        uint64_t curr_file_bytes = 0;
        int num_files = global_info.number_entry;
        char fname[512];
        char ext_fname[512];
        char read_buffer[TRANSFER_SIZE];

        for (int zip_idx = 0; zip_idx < num_files; ++zip_idx)
        {
            if (stop_activity)
                break;
            unzGetCurrentFileInfo(zipfile, &file_info, fname, 512, NULL, 0, NULL, 0);
            sprintf(ext_fname, "%s%s", dest_dir.c_str(), fname);
            const size_t filename_length = strlen(ext_fname);
            bytes_transfered = 0;
            bytes_to_download = file_info.uncompressed_size;
            if (ext_fname[filename_length - 1] != '/')
            {
                snprintf(activity_message, 255, "%s %s: %s", lang_strings[STR_EXTRACTING], file.name, fname);
                curr_file_bytes = 0;
                unzOpenCurrentFile(zipfile);
                FS::MkDirs(ext_fname, true);
                FILE *f = fopen(ext_fname, "wb");
                while (curr_file_bytes < file_info.uncompressed_size)
                {
                    int rbytes = unzReadCurrentFile(zipfile, read_buffer, TRANSFER_SIZE);
                    if (rbytes > 0)
                    {
                        fwrite(read_buffer, 1, rbytes, f);
                        curr_extracted_bytes += rbytes;
                        curr_file_bytes += rbytes;
                        bytes_transfered = curr_file_bytes;
                    }
                }
                fclose(f);
                unzCloseCurrentFile(zipfile);
            }
            else
            {
                FS::MkDirs(ext_fname, true);
            }
            if ((zip_idx + 1) < num_files)
            {
                unzGoToNextFile(zipfile);
            }
        }
        unzClose(zipfile);
        return 1;
    }

    int Extract7Zip(const DirEntry &file, const std::string &dir)
    {
        file_transfering = false;
        FS::MkDirs(dir, true);
        sprintf(filename_extracted, "%s", file.name);
        int res = Extract7zFileEx(file.path, dir.c_str(), callback_7zip, DEFAULT_IN_BUF_SIZE);
        return res == 0;
    }

    int ExtractRar(const DirEntry &file, const std::string &dir)
    {
        file_transfering = false;
        HANDLE hArcData; // Archive Handle
        struct RAROpenArchiveDataEx rarOpenArchiveData;
        struct RARHeaderDataEx rarHeaderData;
        char destPath[256];

        memset(&rarOpenArchiveData, 0, sizeof(rarOpenArchiveData));
        memset(&rarHeaderData, 0, sizeof(rarHeaderData));

        sprintf(destPath, "%s", dir.c_str());
        rarOpenArchiveData.ArcName = (char *)file.path;
        rarOpenArchiveData.CmtBuf = NULL;
        rarOpenArchiveData.CmtBufSize = 0;
        rarOpenArchiveData.OpenMode = RAR_OM_EXTRACT;
        hArcData = RAROpenArchiveEx(&rarOpenArchiveData);

        if (rarOpenArchiveData.OpenResult != ERAR_SUCCESS)
        {
            return 0;
        }

        while (RARReadHeaderEx(hArcData, &rarHeaderData) == ERAR_SUCCESS)
        {
            sprintf(activity_message, "%s %s: %s", lang_strings[STR_EXTRACTING], file.name, rarHeaderData.FileName);
            if (RARProcessFile(hArcData, RAR_EXTRACT, destPath, NULL) != ERAR_SUCCESS)
            {
                RARCloseArchive(hArcData);
                return 0;
            }
        }

        RARCloseArchive(hArcData);
        return 1;
    }

    int Extract(const DirEntry &file, const std::string &dir)
    {
        CompressFileType fileType = getCompressFileType(file.path);

        if (fileType == COMPRESS_FILE_TYPE_ZIP)
            return ExtractZip(file, dir);
        else if (fileType == COMPRESS_FILE_TYPE_7Z)
            return Extract7Zip(file, dir);
        else if (fileType == COMPRESS_FILE_TYPE_RAR)
            return ExtractRar(file, dir);
        else
            sprintf(status_message, "%s - %s", file.name, lang_strings[STR_UNSUPPORTED_FILE_FORMAT]);
        return 1;
    }
}