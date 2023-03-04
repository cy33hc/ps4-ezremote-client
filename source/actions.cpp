#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <minizip/unzip.h>
#include "clients/gdrive.h"
#include "clients/ftpclient.h"
#include "clients/smbclient.h"
#include "clients/webdavclient.h"
#include "clients/apache.h"
#include "clients/nginx.h"
#include "clients/npxserve.h"
#include "clients/iis.h"
#include "common.h"
#include "fs.h"
#include "config.h"
#include "windows.h"
#include "util.h"
#include "lang.h"
#include "actions.h"
#include "installer.h"
#include "web/request.hpp"
#include "web/urn.hpp"
#include "system.h"
#include "zip_util.h"

namespace Actions
{
    static int FtpCallback(int64_t xfered, void *arg)
    {
        bytes_transfered = xfered;
        return 1;
    }

    void RefreshLocalFiles(bool apply_filter)
    {
        multi_selected_local_files.clear();
        local_files.clear();
        int err;
        if (strlen(local_filter) > 0 && apply_filter)
        {
            std::vector<DirEntry> temp_files = FS::ListDir(local_directory, &err);
            std::string lower_filter = Util::ToLower(local_filter);
            for (std::vector<DirEntry>::iterator it = temp_files.begin(); it != temp_files.end();)
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    local_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            local_files = FS::ListDir(local_directory, &err);
        }
        DirEntry::Sort(local_files);
        if (err != 0)
            sprintf(status_message, "%s", lang_strings[STR_FAIL_READ_LOCAL_DIR_MSG]);
    }

    void RefreshRemoteFiles(bool apply_filter)
    {
        if (!remoteclient->Ping())
        {
            remoteclient->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return;
        }

        std::string strList;
        multi_selected_remote_files.clear();
        remote_files.clear();
        if (strlen(remote_filter) > 0 && apply_filter)
        {
            std::vector<DirEntry> temp_files = remoteclient->ListDir(remote_directory);
            std::string lower_filter = Util::ToLower(remote_filter);
            for (std::vector<DirEntry>::iterator it = temp_files.begin(); it != temp_files.end();)
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    remote_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            remote_files = remoteclient->ListDir(remote_directory);
        }
        DirEntry::Sort(remote_files);
    }

    void HandleChangeLocalDirectory(const DirEntry entry)
    {
        if (!entry.isDir)
            return;

        if (strcmp(entry.name, "..") == 0)
        {
            std::string temp_path = std::string(entry.directory);
            if (temp_path.size() > 1)
            {
                if (temp_path.find_last_of("/") == 0)
                {
                    sprintf(local_directory, "%s", "/");
                }
                else
                {
                    sprintf(local_directory, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
                }
            }
            sprintf(local_file_to_select, "%s", temp_path.substr(temp_path.find_last_of("/") + 1).c_str());
        }
        else
        {
            sprintf(local_directory, "%s", entry.path);
        }
        RefreshLocalFiles(false);
        if (strcmp(entry.name, "..") != 0)
        {
            sprintf(local_file_to_select, "%s", local_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleChangeRemoteDirectory(const DirEntry entry)
    {
        if (!entry.isDir)
            return;

        if (!remoteclient->Ping())
        {
            remoteclient->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return;
        }

        if (strcmp(entry.name, "..") == 0)
        {
            std::string temp_path = std::string(entry.directory);
            if (temp_path.size() > 1)
            {
                if (temp_path.find_last_of("/") == 0)
                {
                    sprintf(remote_directory, "%s", "/");
                }
                else
                {
                    sprintf(remote_directory, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
                }
            }
            sprintf(remote_file_to_select, "%s", temp_path.substr(temp_path.find_last_of("/") + 1).c_str());
        }
        else
        {
            sprintf(remote_directory, "%s", entry.path);
        }
        RefreshRemoteFiles(false);
        if (strcmp(entry.name, "..") != 0)
        {
            sprintf(remote_file_to_select, "%s", remote_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleRefreshLocalFiles()
    {
        int prev_count = local_files.size();
        RefreshLocalFiles(false);
        int new_count = local_files.size();
        if (prev_count != new_count)
        {
            sprintf(local_file_to_select, "%s", local_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleRefreshRemoteFiles()
    {
        int prev_count = remote_files.size();
        RefreshRemoteFiles(false);
        int new_count = remote_files.size();
        if (prev_count != new_count)
        {
            sprintf(remote_file_to_select, "%s", remote_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void CreateNewLocalFolder(char *new_folder)
    {
        sprintf(status_message, "%s", "");
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = FS::GetPath(local_directory, folder);
        FS::MkDirs(path);
        RefreshLocalFiles(false);
        sprintf(local_file_to_select, "%s", folder.c_str());
    }

    void CreateNewRemoteFolder(char *new_folder)
    {
        sprintf(status_message, "%s", "");
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = remoteclient->GetPath(remote_directory, folder.c_str());
        if (remoteclient->Mkdir(path.c_str()))
        {
            RefreshRemoteFiles(false);
            sprintf(remote_file_to_select, "%s", folder.c_str());
        }
        else
        {
            sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], remoteclient->LastResponse());
        }
    }

    void RenameLocalFolder(const char *old_path, const char *new_path)
    {
        sprintf(status_message, "%s", "");
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(local_directory, new_name);
        FS::Rename(old_path, path);
        RefreshLocalFiles(false);
        sprintf(local_file_to_select, "%s", new_name.c_str());
    }

    void RenameRemoteFolder(const char *old_path, const char *new_path)
    {
        sprintf(status_message, "%s", "");
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(remote_directory, new_name);
        if (remoteclient->Rename(old_path, path.c_str()))
        {
            RefreshRemoteFiles(false);
            sprintf(remote_file_to_select, "%s", new_name.c_str());
        }
        else
        {
            sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], remoteclient->LastResponse());
        }
    }

    void *DeleteSelectedLocalFilesThread(void *argp)
    {
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            FS::RmRecursive(it->path);
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void DeleteSelectedLocalFiles()
    {
        int ret = pthread_create(&bk_activity_thid, NULL, DeleteSelectedLocalFilesThread, NULL);
        if (ret != 0)
        {
            activity_inprogess = false;
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
    }

    void *DeleteSelectedRemotesFilesThread(void *argp)
    {
        if (remoteclient->Ping())
        {
            std::vector<DirEntry> files;
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_remote_file);

            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
            {
                if (it->isDir)
                    remoteclient->Rmdir(it->path, true);
                else
                {
                    sprintf(activity_message, "%s %s", lang_strings[STR_DELETING], it->path);
                    if (!remoteclient->Delete(it->path))
                    {
                        sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], remoteclient->LastResponse());
                    }
                }
            }
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
        else
        {
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            Disconnect();
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        return NULL;
    }

    void DeleteSelectedRemotesFiles()
    {
        int res = pthread_create(&bk_activity_thid, NULL, DeleteSelectedRemotesFilesThread, NULL);
        if (res != 0)
        {
            activity_inprogess = false;
            Windows::SetModalMode(false);
        }
    }

    int UploadFile(const char *src, const char *dest)
    {
        int ret;
        if (!remoteclient->Ping())
        {
            remoteclient->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return 0;
        }

        if (overwrite_type == OVERWRITE_PROMPT && remoteclient->FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && remoteclient->FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            return remoteclient->Put(src, dest);
        }

        return 1;
    }

    int Upload(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            remoteclient->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    remoteclient->Mkdir(new_path);
                    ret = Upload(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_UPLOADING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    ret = UploadFile(entries[i].path, new_path);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_UPLOAD_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_UPLOADING], src.name);
            bytes_to_download = src.file_size;
            ret = UploadFile(src.path, new_path);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_UPLOAD_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *UploadFilesThread(void *argp)
    {
        file_transfering = true;
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
                Upload(*it, new_dir);
            }
            else
            {
                Upload(*it, remote_directory);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void UploadFiles()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, UploadFilesThread, NULL);
        if (res != 0)
        {
            activity_inprogess = false;
            file_transfering = false;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
    }

    int DownloadFile(const char *src, const char *dest)
    {
        bytes_transfered = 0;
        if (!remoteclient->Size(src, &bytes_to_download))
        {
            remoteclient->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return 0;
        }

        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && FS::FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            return remoteclient->Get(dest, src);
        }

        return 1;
    }

    int Download(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = remoteclient->ListDir(src.path);
            FS::MkDirs(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    FS::MkDirs(new_path);
                    ret = Download(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DOWNLOADING], entries[i].path);
                    ret = DownloadFile(entries[i].path, new_path);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DOWNLOAD_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DOWNLOADING], src.path);
            ret = DownloadFile(src.path, new_path);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_DOWNLOAD_MSG], src.path);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *DownloadFilesThread(void *argp)
    {
        file_transfering = true;
        std::vector<DirEntry> files;
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_remote_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                Download(*it, new_dir);
            }
            else
            {
                Download(*it, local_directory);
            }
        }
        file_transfering = false;
        activity_inprogess = false;
        multi_selected_remote_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void DownloadFiles()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, DownloadFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
    }

    void *InstallRemotePkgsThread(void *argp)
    {
        int failed = 0;
        int success = 0;
        int skipped = 0;

        std::vector<DirEntry> files;
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_remote_file);

        bool download_and_install = false;
        if (remote_settings->enable_rpi)
        {
            std::string url = INSTALLER::getRemoteUrl(files.begin()->path);
            sprintf(activity_message, "%s", lang_strings[STR_CHECKING_REMOTE_SERVER_MSG]);
            file_transfering = false;
            if (!INSTALLER::canInstallRemotePkg(url))
            {
                confirm_state = CONFIRM_WAIT;
                action_to_take = selected_action;
                activity_inprogess = false;
                while (confirm_state == CONFIRM_WAIT)
                {
                    sceKernelUsleep(100000);
                }
                activity_inprogess = true;
                selected_action = action_to_take;

                if (confirm_state == CONFIRM_YES)
                {
                    download_and_install = true;
                }
                else
                {
                    goto finish;
                }
            }
        }
        else
        {
            download_and_install = true;
        }
        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (stop_activity)
                break;
            sprintf(activity_message, "%s %s", lang_strings[STR_INSTALLING], it->name);

            if (!it->isDir)
            {
                std::string path = std::string(it->path);
                path = Util::ToLower(path);
                if (path.size() > 4 && path.substr(path.size() - 4) == ".pkg")
                {
                    pkg_header header;
                    memset(&header, 0, sizeof(header));

                    if (remoteclient->Head(it->path, (void *)&header, sizeof(header)) == 0)
                        failed++;
                    else
                    {
                        if (BE32(header.pkg_magic) == PKG_MAGIC)
                        {
                            if (download_and_install)
                            {
                                if (DownloadAndInstallPkg(it->path, &header) == 0)
                                    failed++;
                                else
                                    success++;
                            }
                            else
                            {
                                if (INSTALLER::InstallRemotePkg(it->path, &header) == 0)
                                    failed++;
                                else
                                    success++;
                            }
                        }
                        else
                            skipped++;
                    }
                }
                else
                    skipped++;
            }
            else
                skipped++;

            sprintf(status_message, "%s %s = %d, %s = %d, %s = %d", lang_strings[STR_INSTALL],
                    lang_strings[STR_INSTALL_SUCCESS], success, lang_strings[STR_INSTALL_FAILED], failed,
                    lang_strings[STR_INSTALL_SKIPPED], skipped);
        }
    finish:
        activity_inprogess = false;
        multi_selected_remote_files.clear();
        Windows::SetModalMode(false);
        return NULL;
    }

    void InstallRemotePkgs()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, InstallRemotePkgsThread, NULL);
        if (res != 0)
        {
            activity_inprogess = false;
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *InstallLocalPkgsThread(void *argp)
    {
        int failed = 0;
        int success = 0;
        int skipped = 0;
        int ret;

        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (stop_activity)
                break;
            sprintf(activity_message, "%s %s", lang_strings[STR_INSTALLING], it->name);

            if (!it->isDir)
            {
                std::string path = std::string(it->path);
                path = Util::ToLower(path);
                if (path.size() > 4 && path.substr(path.size() - 4) == ".pkg")
                {
                    pkg_header header;
                    memset(&header, 0, sizeof(header));
                    if (FS::Head(it->path, (void *)&header, sizeof(header)) == 0)
                        failed++;
                    else
                    {
                        if (BE32(header.pkg_magic) == PKG_MAGIC)
                        {
                            if ((ret = INSTALLER::InstallLocalPkg(it->path, &header)) <= 0)
                            {
                                if (ret == -1)
                                {
                                    sprintf(activity_message, "%s - %s", it->name, lang_strings[STR_INSTALL_FROM_DATA_MSG]);
                                    sceKernelUsleep(3000000);
                                }
                                else if (ret == -2)
                                {
                                    sprintf(activity_message, "%s - %s", it->name, lang_strings[STR_ALREADY_INSTALLED_MSG]);
                                    sceKernelUsleep(3000000);
                                }
                                failed++;
                            }
                            else
                                success++;
                        }
                        else
                            skipped++;
                    }
                }
                else
                    skipped++;
            }
            else
                skipped++;

            sprintf(status_message, "%s %s = %d, %s = %d, %s = %d", lang_strings[STR_INSTALL],
                    lang_strings[STR_INSTALL_SUCCESS], success, lang_strings[STR_INSTALL_FAILED], failed,
                    lang_strings[STR_INSTALL_SKIPPED], skipped);
        }
        activity_inprogess = false;
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        return NULL;
    }

    void InstallLocalPkgs()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, InstallLocalPkgsThread, NULL);
        if (res != 0)
        {
            activity_inprogess = false;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *ExtractZipThread(void *argp)
    {
        FS::MkDirs(extract_zip_folder, true);
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (stop_activity)
                break;
            if (!it->isDir)
            {
                int ret = ZipUtil::Extract(*it, extract_zip_folder);
                if (ret == 0)
                {
                    sprintf(status_message, "%s %s", lang_strings[STR_FAILED_TO_EXTRACT], it->name);
                    sceKernelUsleep(100000);
                }
            }
        }
        activity_inprogess = false;
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void ExtractLocalZips()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, ExtractZipThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *MakeZipThread(void *argp)
    {
        zipFile zf = zipOpen64(zip_file_path, APPEND_STATUS_CREATE);
        if (zf != NULL)
        {
            std::vector<DirEntry> files;
            if (multi_selected_local_files.size() > 0)
                std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_local_file);

            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
            {
                if (stop_activity)
                    break;
                int res = ZipUtil::ZipAddPath(zf, it->path, strlen(it->directory) + 1, Z_DEFAULT_COMPRESSION);
                if (res <= 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_ERROR_CREATE_ZIP]);
                    sceKernelUsleep(1000000);
                }
            }
            zipClose(zf, NULL);
        }
        else
        {
            sprintf(status_message, "%s", lang_strings[STR_ERROR_CREATE_ZIP]);
        }
        activity_inprogess = false;
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void MakeLocalZip()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, MakeZipThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    std::string GetGoogleDownloadUrl(std::string &url)
    {
        size_t scheme_pos = url.find_first_of("://");
        size_t path_pos = url.find_first_of("/", scheme_pos + 3);
        std::string host = url.substr(0, path_pos);
        std::string path = url.substr(path_pos);

        std::string first_download_path;
        size_t file_id_start_pos = path.find("/file/d/");
        if (file_id_start_pos != std::string::npos)
        {
            file_id_start_pos = file_id_start_pos + 8;
            std::string file_id = path.substr(file_id_start_pos);
            size_t file_id_end_pos = file_id.find_first_of("/");
            file_id = file_id.substr(0, file_id_end_pos);
            first_download_path = "/uc?export=download&id=" + file_id;
        }
        else if (path.find("/uc?export=download") != std::string::npos)
        {
            first_download_path = path;
        }
        else
        {
            return "";
        }

        WebDAV::WebDavClient tmp_client;
        tmp_client.Connect(host.c_str(), "", "", false);
        WebDAV::dict_t headers;
        tmp_client.GetHeaders(first_download_path.c_str(), &headers);

        std::string content_type = WebDAV::get(headers, "content-type");
        if (content_type.find("application/octet-stream") != std::string::npos)
        {
            return first_download_path;
        }
        else if (content_type.find("text/html") == std::string::npos)
        {
            return "";
        }
        char *buffer_ptr = nullptr;
        unsigned long long buffer_size = 0;
        tmp_client.GetClient()->download_to(first_download_path, buffer_ptr, buffer_size);

        lxb_status_t status;
        lxb_dom_element_t *element;
        lxb_html_document_t *document;
        lxb_dom_collection_t *collection;
        lxb_dom_attr_t *attr;
        document = lxb_html_document_create();
        status = lxb_html_document_parse(document, (lxb_char_t *)buffer_ptr, buffer_size);
        if (status != LXB_STATUS_OK)
            return "";
        collection = lxb_dom_collection_make(&document->dom_document, 128);
        if (collection == NULL)
        {
            return "";
        }
        status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                              collection, (const lxb_char_t *)"form", 4);
        if (status != LXB_STATUS_OK)
            return "";
        std::string download_url;
        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++)
        {
            element = lxb_dom_collection_element(collection, i);
            std::string form_id((char *)element->attr_id->value->data, element->attr_id->value->length);
            if (form_id == "download-form")
            {
                size_t value_len;
                const lxb_char_t *value = lxb_dom_element_get_attribute(element, (const lxb_char_t *)"action", 6, &value_len);
                download_url = std::string((char *)value, value_len);
                break;
            }
        }
        lxb_dom_collection_destroy(collection, true);
        lxb_html_document_destroy(document);

        return download_url;
    }

    void *InstallUrlPkgThread(void *argp)
    {
        bytes_transfered = 0;
        sprintf(status_message, "%s", "");
        pkg_header header;
        char filename[2000];
        OrbisDateTime now;
        OrbisTick tick;
        sceRtcGetCurrentClockLocalTime(&now);
        sceRtcGetTick(&now, &tick);
        sprintf(filename, "%s/%lu.pkg", DATA_PATH, tick.mytick);

        std::string full_url = std::string(install_pkg_url.url);
        if (full_url.find("https://drive.google.com") != std::string::npos)
            full_url = GetGoogleDownloadUrl(full_url);

        if (full_url.empty())
        {
            sprintf(status_message, "%s", lang_strings[STR_FAIL_TO_OBTAIN_GG_DL_MSG]);
            activity_inprogess = false;
            Windows::SetModalMode(false);
            return NULL;
        }
        size_t scheme_pos = full_url.find_first_of("://");
        size_t path_pos = full_url.find_first_of("/", scheme_pos + 3);
        std::string host = full_url.substr(0, path_pos);
        std::string path = full_url.substr(path_pos);

        WebDAV::WebDavClient tmp_client;
        tmp_client.Connect(host.c_str(), install_pkg_url.username, install_pkg_url.password, false);

        sprintf(activity_message, "%s URL to %s", lang_strings[STR_DOWNLOADING], filename);
        int s = sizeof(pkg_header);
        memset(&header, 0, s);
        WebDAV::dict_t response_headers{};
        int ret = tmp_client.GetHeaders(path.c_str(), &response_headers);
        if (!ret)
        {
            sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], lang_strings[STR_CANNOT_READ_PKG_HDR_MSG]);
            tmp_client.Quit();
            activity_inprogess = false;
            Windows::SetModalMode(false);
            return NULL;
        }

        auto content_length = WebDAV::get(response_headers, "content-length");
        if (content_length.length() > 0)
            bytes_to_download = std::stol(content_length);
        else
            bytes_to_download = 1;
        file_transfering = 1;
        int is_performed = tmp_client.Get(filename, path.c_str());

        if (is_performed == 0)
        {
            sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], tmp_client.LastResponse());
            tmp_client.Quit();
            activity_inprogess = false;
            Windows::SetModalMode(false);
            return NULL;
        }
        tmp_client.Quit();

        FILE *in = FS::OpenRead(filename);
        if (in == NULL)
        {
            sprintf(status_message, "%s - Error opening temp pkg file - %s", lang_strings[STR_FAILED], filename);
            activity_inprogess = false;
            Windows::SetModalMode(false);
            return NULL;
        }

        FS::Read(in, (void *)&header, s);
        FS::Close(in);
        if (BE32(header.pkg_magic) == PKG_MAGIC)
        {
            int ret;
            if ((ret = INSTALLER::InstallLocalPkg(filename, &header, true)) != 1)
            {
                if (ret == -1)
                {
                    sprintf(activity_message, "%s", lang_strings[STR_INSTALL_FROM_DATA_MSG]);
                    sceKernelUsleep(3000000);
                }
                else if (ret == -2)
                {
                    sprintf(activity_message, "%s", lang_strings[STR_ALREADY_INSTALLED_MSG]);
                    sceKernelUsleep(3000000);
                }
                else if (ret == -3)
                {
                    sprintf(activity_message, "%s", lang_strings[STR_FAIL_INSTALL_TMP_PKG_MSG]);
                    sceKernelUsleep(5000000);
                }
                if (ret != -3 && auto_delete_tmp_pkg)
                    FS::Rm(filename);
            }
        }

        activity_inprogess = false;
        Windows::SetModalMode(false);
        return NULL;
    }

    void InstallUrlPkg()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, InstallUrlPkgThread, NULL);
        if (res != 0)
        {
            activity_inprogess = false;
            Windows::SetModalMode(false);
        }
    }

    void Connect()
    {
        CONFIG::SaveConfig();

        if (strncmp(remote_settings->server, GOOGLE_DRIVE_BASE_URL, strlen(GOOGLE_DRIVE_BASE_URL)) == 0)
        {
            remoteclient = new GDriveClient();
            remoteclient->Connect("", "", "");
        }
        else if (strncmp(remote_settings->server, "https://", 8) == 0 || strncmp(remote_settings->server, "http://", 7) == 0)
        {
            if (strcmp(remote_settings->http_server_type, HTTP_SERVER_APACHE) == 0)
                remoteclient = new ApacheClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_MS_IIS) == 0)
                remoteclient = new IISClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_NGINX) == 0)
                remoteclient = new NginxClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_NPX_SERVE) == 0)
                remoteclient = new NpxServeClient();
        }
        else if (strncmp(remote_settings->server, "webdavs://", 10) == 0 || strncmp(remote_settings->server, "webdav://", 9) == 0)
        {
            remoteclient = new WebDAV::WebDavClient();
        }
        else if (strncmp(remote_settings->server, "smb://", 6) == 0)
        {
            remoteclient = new SmbClient();
        }
        else if (strncmp(remote_settings->server, "ftp://", 6) == 0)
        {
            FtpClient *client = new FtpClient();
            client->SetConnmode(FtpClient::pasv);
            client->SetCallbackBytes(1);
            client->SetCallbackXferFunction(FtpCallback);
            remoteclient = client;
        }
        else
        {
            sprintf(status_message, "%s", lang_strings[STR_PROTOCOL_NOT_SUPPORTED]);
            selected_action = ACTION_NONE;
            return;
        }

        if (remoteclient->Connect(remote_settings->server, remote_settings->username, remote_settings->password))
        {
            RefreshRemoteFiles(false);

            if (remoteclient->clientType() == CLIENT_TYPE_FTP)
            {
                int res = pthread_create(&ftp_keep_alive_thid, NULL, KeepAliveThread, NULL);
            }
        }
        else
        {
            sprintf(status_message, "%s - %s", lang_strings[STR_FAIL_TIMEOUT_MSG], remoteclient->LastResponse());
            if (remoteclient != nullptr)
            {
                remoteclient->Quit();
                delete remoteclient;
                remoteclient = nullptr;
            }
        }
        selected_action = ACTION_NONE;
    }

    void Disconnect()
    {
        if (remoteclient != nullptr)
        {
            if (remoteclient->IsConnected())
                remoteclient->Quit();
            multi_selected_remote_files.clear();
            remote_files.clear();
            sprintf(remote_directory, "%s", "/");
            sprintf(status_message, "%s", "");
            delete remoteclient;
            remoteclient = nullptr;
        }
    }

    void SelectAllLocalFiles()
    {
        for (int i = 0; i < local_files.size(); i++)
        {
            if (strcmp(local_files[i].name, "..") != 0)
                multi_selected_local_files.insert(local_files[i]);
        }
    }

    void SelectAllRemoteFiles()
    {
        for (int i = 0; i < remote_files.size(); i++)
        {
            if (strcmp(remote_files[i].name, "..") != 0)
                multi_selected_remote_files.insert(remote_files[i]);
        }
    }

    void *KeepAliveThread(void *argp)
    {
        long idle;
        while (remoteclient != nullptr && remoteclient->clientType() == CLIENT_TYPE_FTP && remoteclient->IsConnected())
        {
            FtpClient *client = (FtpClient *)remoteclient;
            idle = client->GetIdleTime();
            if (idle > 60000000)
            {
                if (!client->Ping())
                {
                    client->Quit();
                    sprintf(status_message, "%s", lang_strings[STR_REMOTE_TERM_CONN_MSG]);
                    return NULL;
                }
            }
            sceKernelUsleep(500000);
        }
        return NULL;
    }

    int CopyOrMoveLocalFile(const char *src, const char *dest, bool isCopy)
    {
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && FS::FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            if (isCopy)
                return FS::Copy(src, dest);
            else
                return FS::Move(src, dest);
        }

        return 1;
    }

    int CopyOrMove(const DirEntry &src, const char *dest, bool isCopy)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            FS::MkDirs(dest, true);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    FS::MkDirs(new_path, true);
                    ret = CopyOrMove(entries[i], new_path, isCopy);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", isCopy ? lang_strings[STR_COPYING] : lang_strings[STR_MOVING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    ret = CopyOrMoveLocalFile(entries[i].path, new_path, isCopy);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", isCopy ? lang_strings[STR_COPYING] : lang_strings[STR_MOVING], src.name);
            bytes_to_download = src.file_size;
            ret = CopyOrMoveLocalFile(src.path, new_path, isCopy);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", isCopy ? lang_strings[STR_FAIL_COPY_MSG] : lang_strings[STR_FAIL_MOVE_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *MoveLocalFilesThread(void *argp)
    {
        file_transfering = true;
        for (std::vector<DirEntry>::iterator it = local_paste_files.begin(); it != local_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, local_directory) == 0)
                continue;

            if (it->isDir)
            {
                if (strncmp(local_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                CopyOrMove(*it, new_dir, false);
                FS::RmRecursive(it->path);
            }
            else
            {
                CopyOrMove(*it, local_directory, false);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void MoveLocalFiles()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, MoveLocalFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *CopyLocalFilesThread(void *argp)
    {
        file_transfering = true;
        for (std::vector<DirEntry>::iterator it = local_paste_files.begin(); it != local_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, local_directory) == 0)
                continue;

            if (it->isDir)
            {
                if (strncmp(local_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_COPY_TO_SUBDIR_MSG]);
                    continue;
                }
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", local_directory, FS::hasEndSlash(local_directory) ? "" : "/", it->name);
                CopyOrMove(*it, new_dir, true);
            }
            else
            {
                CopyOrMove(*it, local_directory, true);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void CopyLocalFiles()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, CopyLocalFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyOrMoveRemoteFile(const std::string &src, const std::string &dest, bool isCopy)
    {
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && remoteclient->FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest.c_str());
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            activity_inprogess = false;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            activity_inprogess = true;
            selected_action = action_to_take;
        }
        else if (overwrite_type == OVERWRITE_NONE && remoteclient->FileExists(dest))
        {
            confirm_state = CONFIRM_NO;
        }
        else
        {
            confirm_state = CONFIRM_YES;
        }

        if (confirm_state == CONFIRM_YES)
        {
            if (isCopy)
                return remoteclient->Copy(src, dest);
            else
                return remoteclient->Move(src, dest);
        }

        return 1;
    }

    void *MoveRemoteFilesThread(void *argp)
    {
        file_transfering = false;
        for (std::vector<DirEntry>::iterator it = remote_paste_files.begin(); it != remote_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, remote_directory) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(remote_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
            }

            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_MOVING], it->path);
            int res = CopyOrMoveRemoteFile(it->path, new_path, false);
            if (res == 0)
                sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
        }
        activity_inprogess = false;
        file_transfering = false;
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void MoveRemoteFiles()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, MoveRemoteFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            remote_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyRemotePath(const DirEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = remoteclient->ListDir(src.path);
            remoteclient->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (stop_activity)
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    FS::MkDirs(new_path, true);
                    ret = CopyRemotePath(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "%s %s", lang_strings[STR_COPYING], entries[i].path);
                    bytes_to_download = entries[i].file_size;
                    bytes_transfered = 0;
                    ret = CopyOrMoveRemoteFile(entries[i].path, new_path, true);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "%s %s", lang_strings[STR_FAIL_COPY_MSG], entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", lang_strings[STR_COPYING], src.name);
            ret = CopyOrMoveRemoteFile(src.path, new_path, true);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "%s %s", lang_strings[STR_FAIL_COPY_MSG], src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *CopyRemoteFilesThread(void *argp)
    {
        file_transfering = false;
        for (std::vector<DirEntry>::iterator it = remote_paste_files.begin(); it != remote_paste_files.end(); ++it)
        {
            if (stop_activity)
                break;

            if (strcmp(it->directory, remote_directory) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(remote_directory, it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_COPY_TO_SUBDIR_MSG]);
                    continue;
                }
                int res = CopyRemotePath(*it, new_path);
                if (res == 0)
                    sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
            else
            {
                int res = CopyRemotePath(*it, remote_directory);
                if (res == 0)
                    sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
        }
        activity_inprogess = false;
        file_transfering = false;
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void CopyRemoteFiles()
    {
        sprintf(status_message, "%s", "");
        int res = pthread_create(&bk_activity_thid, NULL, CopyRemoteFilesThread, NULL);
        if (res != 0)
        {
            file_transfering = false;
            activity_inprogess = false;
            remote_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int DownloadAndInstallPkg(const std::string &filename, pkg_header *header)
    {
        char local_file[2000];
        OrbisDateTime now;
        OrbisTick tick;
        sceRtcGetCurrentClockLocalTime(&now);
        sceRtcGetTick(&now, &tick);
        sprintf(local_file, "%s/%lu.pkg", DATA_PATH, tick.mytick);

        sprintf(activity_message, "%s %s to %s", lang_strings[STR_DOWNLOADING], filename.c_str(), local_file);
        remoteclient->Size(filename, &bytes_to_download);
        bytes_transfered = 0;
        file_transfering = true;
        int ret = remoteclient->Get(local_file, filename);
        if (ret == 0)
            return 0;

        return INSTALLER::InstallLocalPkg(local_file, header, true);
    }

    void CreateLocalFile(char *filename)
    {
        std::string new_file = FS::GetPath(local_directory, filename);
        std::string temp_file = new_file;
        int i = 1;
        while (true)
        {
            if (!FS::FileExists(temp_file))
                break;
            temp_file = new_file + "." + std::to_string(i);
            i++;
        }
        FILE* f = FS::Create(temp_file);
        FS::Close(f);
        RefreshLocalFiles(false);
        sprintf(local_file_to_select, "%s", temp_file.c_str());
    }

    void CreateRemoteFile(char *filename)
    {
        std::string new_file = FS::GetPath(remote_directory, filename);
        std::string temp_file = new_file;
        int i = 1;
        while (true)
        {
            if (!remoteclient->FileExists(temp_file))
                break;
            temp_file = new_file + "." + std::to_string(i);
            i++;
        }

        OrbisTick tick;
        sceRtcGetCurrentTick(&tick);
        std::string local_tmp = std::string(DATA_PATH) + "/" + std::to_string(tick.mytick);
        FILE *f = FS::Create(local_tmp);
        FS::Close(f);
        remoteclient->Put(local_tmp, temp_file);
        FS::Rm(local_tmp);
        RefreshRemoteFiles(false);
        sprintf(remote_file_to_select, "%s", temp_file.c_str());
    }

}
