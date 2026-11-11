#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <json-c/json.h>
#include "json_object_ref.h"
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <minizip/unzip.h>
#include <orbis/SystemService.h>
#include "clients/gdrive.h"
#include "clients/ftpclient.h"
#include "clients/smbclient.h"
#include "clients/webdav.h"
#include "clients/apache.h"
#include "clients/archiveorg.h"
#include "clients/github.h"
#include "clients/myrient.h"
#include "clients/nginx.h"
#include "clients/npxserve.h"
#include "clients/nfsclient.h"
#include "clients/iis.h"
#include "clients/rclone.h"
#include "clients/sftpclient.h"
#include "filehost/filehost.h"
#include "server/http_server.h"
#include "common.h"
#include "fs.h"
#include "config.h"
#include "windows.h"
#include "util.h"
#include "activity.h"
#include "lang.h"
#include "actions.h"
#include "installer.h"
#include "system.h"
#include "sfo.h"
#include "zip_util.h"

namespace Actions
{
    // Per-operation inputs handed to worker threads, so a second activity
    // cannot change the destination or selection of a running one.
    struct TransferJob
    {
        std::vector<DirEntry> files;
        std::string target;
    };

    // Per-operation inputs for archive jobs, so two extract/compress
    // operations cannot overwrite each other's destination.
    struct ArchiveJob
    {
        std::vector<DirEntry> files;
        std::string target;
    };

    static int FtpCallback(int64_t xfered, void *arg)
    {
        if (arg != nullptr)
        {
            RemoteClient *client = static_cast<RemoteClient *>(arg);
            client->SetTransferredBytes((uint64_t) xfered);
            Activities::ProgressCurrent(client->TransferredBytes(), client->TotalBytes());
        }
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
        if (remoteclient != nullptr)
        {
            int prev_count = remote_files.size();
            RefreshRemoteFiles(false);
            int new_count = remote_files.size();
            if (prev_count != new_count)
            {
                sprintf(remote_file_to_select, "%s", remote_files[0].name);
            }
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
        Activities::Guard activity(ACTIVITY_DELETE, STR_DELETING, ACTIVITY_NATIVE, ACTIVITY_LOCAL);
        std::vector<DirEntry> files;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(files));
        else
            files.push_back(selected_local_file);

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            Activities::ItemCurrent(it->path);
            FS::RmRecursive(it->path);
        }
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void DeleteSelectedLocalFiles()
    {
        int ret = pthread_create(&bk_activity_thid, NULL, DeleteSelectedLocalFilesThread, NULL);
        if (ret != 0)
        {
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
    }

    void *DeleteSelectedRemotesFilesThread(void *argp)
    {
        Activities::Guard activity(ACTIVITY_DELETE, STR_DELETING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        if (remoteclient->Ping())
        {
            std::vector<DirEntry> files;
            if (multi_selected_remote_files.size() > 0)
                std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(files));
            else
                files.push_back(selected_remote_file);

            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
            {
                Activities::ItemCurrent(it->path);
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
        Windows::SetModalMode(false);
        return NULL;
    }

    void DeleteSelectedRemotesFiles()
    {
        int res = pthread_create(&bk_activity_thid, NULL, DeleteSelectedRemotesFilesThread, NULL);
        if (res != 0)
        {
            Windows::SetModalMode(false);
        }
    }

    int UploadFile(const char *src, const char *dest)
    {
        if (Activities::Stopped())
            return 0;
        int ret;
        if (!remoteclient->Ping())
        {
            remoteclient->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return 0;
        }

        int64_t upload_size = FS::GetSize(src);
        if (upload_size > 0)
            remoteclient->SetTotalBytes((uint64_t) upload_size);

        if (overwrite_type == OVERWRITE_PROMPT && remoteclient->FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_kind = CONFIRM_KIND_OVERWRITE;
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            selected_action = action_to_take;
            if (confirm_state == CONFIRM_ALL)
            {
                dont_prompt_overwrite = true;
                confirm_state = CONFIRM_YES;
            }
            else if (confirm_state == CONFIRM_CANCEL)
            {
                Activities::Cancel(Activities::CurrentId());
                confirm_state = CONFIRM_NO;
            }
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

        sceSystemServicePowerTick();
        return 1;
    }

    int Upload(const DirEntry &src, const char *dest)
    {
        if (Activities::Stopped())
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            remoteclient->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (Activities::Stopped())
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
                    remoteclient->SetTotalBytes(entries[i].file_size);
                    remoteclient->ResetTransferredBytes();
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
            remoteclient->SetTotalBytes(src.file_size);
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
        TransferJob *job = static_cast<TransferJob *>(argp);
        Activities::Guard activity(ACTIVITY_UPLOAD, STR_UPLOADING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;

            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", target.c_str(), FS::hasEndSlash(target.c_str()) ? "" : "/", it->name);
                Upload(*it, new_dir);
            }
            else
            {
                Upload(*it, target.c_str());
            }
        }
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void UploadFiles()
    {
        sprintf(status_message, "%s", "");
        TransferJob *job = new TransferJob();
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(job->files));
        else
            job->files.push_back(selected_local_file);
        job->target = remote_directory;

        int res = pthread_create(&bk_activity_thid, NULL, UploadFilesThread, job);
        if (res != 0)
        {
            delete job;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_REMOTE_FILES;
        }
    }

    int BackgroundDownload(const char *src, const char *dest, uint64_t file_size)
    {
        uint64_t id = Util::GetTick();
        json_object *params = json_object_new_object();
        JsonObjectRef params_ref(params);
        std::string _src = std::string(src);

        json_object_object_add(params, "type", json_object_new_int(remote_settings->type));
        json_object_object_add(params, "dest_path", json_object_new_string(dest));
        json_object_object_add(params, "size", json_object_new_uint64(file_size));
        json_object_object_add(params, "id", json_object_new_uint64(id));

        if (remote_settings->type == CLIENT_TYPE_GOOGLE)
        {
            json_object_object_add(params, "username", json_object_new_string(gg_app.client_id));
        }
        else
        {
            json_object_object_add(params, "username", json_object_new_string(remote_settings->username));
        }

        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER && strcmp(remote_settings->http_server_type, HTTP_SERVER_GITHUB) == 0)
        {
            GithubClient *tmp_client = (GithubClient*) remoteclient;
            _src = tmp_client->GetRedirectUrl(src);
        }
        else if (remote_settings->type == CLIENT_TYPE_GOOGLE)
        {
            GDriveClient *tmp_client = (GDriveClient*) remoteclient;
            _src = tmp_client->GetDownloadUrl(src);
        }
        json_object_object_add(params, "src_path", json_object_new_string(_src.c_str()));

        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER && strcmp(remote_settings->http_server_type, HTTP_SERVER_GITHUB) == 0)
		{
			json_object_object_add(params, "url", json_object_new_string("https://release-assets.githubusercontent.com"));
		}
		else if (remote_settings->type == CLIENT_TYPE_GOOGLE)
		{
			json_object_object_add(params, "url", json_object_new_string(GOOGLE_API_URL));
		}
		else
		{
			json_object_object_add(params, "url", json_object_new_string(remote_settings->server));
		}

		if (remote_settings->type == CLIENT_TYPE_GOOGLE && (remoteclient != nullptr && remoteclient->clientType() == CLIENT_TYPE_GOOGLE))
		{
			GDriveClient* tmp_client = (GDriveClient*) remoteclient;
			json_object_object_add(params, "password", json_object_new_string(gg_app.client_secret));
			json_object_object_add(params, "refresh_token", json_object_new_string(remote_settings->gg_account.refresh_token));
		}
		else
		{
			json_object_object_add(params, "password", json_object_new_string(remote_settings->password));
		}

        if (remote_settings->type == CLIENT_TYPE_HTTP_SERVER)
        {
            json_object_object_add(params, "http_server_type", json_object_new_string(remote_settings->http_server_type));
        }

        const char *params_str = json_object_to_json_string(params);

        httplib::Client tmp_client = httplib::Client("http://127.0.0.1:" + std::to_string(http_int_server_port));
        if (auto res = tmp_client.Post("/download_url", params_str, strlen(params_str), "application/json"))
        {
            if (HTTP_SUCCESS(res->status))
            {
                Util::Notify("%s queued for download", src);
                return 1;
            }
            else
            {
                Util::Notify("Failed to queue %s for download in background", src);
                return 0;
            }
        }

        return 0;
    }

    int DownloadFile(const char *src, const char *dest)
    {
        if (Activities::Stopped())
            return 0;
        remoteclient->ResetTransferredBytes();
        uint64_t total_size = 0;
        if (!remoteclient->Size(src, &total_size))
        {
            remoteclient->Quit();
            sprintf(status_message, "%s", lang_strings[STR_CONNECTION_CLOSE_ERR_MSG]);
            return 0;
        }
        remoteclient->SetTotalBytes(total_size);

        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_kind = CONFIRM_KIND_OVERWRITE;
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            selected_action = action_to_take;
            if (confirm_state == CONFIRM_ALL)
            {
                dont_prompt_overwrite = true;
                confirm_state = CONFIRM_YES;
            }
            else if (confirm_state == CONFIRM_CANCEL)
            {
                Activities::Cancel(Activities::CurrentId());
                confirm_state = CONFIRM_NO;
            }
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
            if (enable_background_download && remoteclient->TotalBytes() > minimum_backgrond_file_size)
            {
                return BackgroundDownload(src, dest, remoteclient->TotalBytes());
            }
            return remoteclient->Get(dest, src);
        }

        sceSystemServicePowerTick();
        return 1;
    }

    int Download(const DirEntry &src, const char *dest)
    {
        if (Activities::Stopped())
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = remoteclient->ListDir(src.path);
            FS::MkDirs(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (Activities::Stopped())
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
        TransferJob *job = static_cast<TransferJob *>(argp);
        Activities::Guard activity(ACTIVITY_DOWNLOAD, STR_DOWNLOADING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;

            Activities::ItemCurrent(it->path);
            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", target.c_str(), FS::hasEndSlash(target.c_str()) ? "" : "/", it->name);
                Download(*it, new_dir);
            }
            else
            {
                Download(*it, target.c_str());
            }
        }
        multi_selected_remote_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void DownloadFiles()
    {
        sprintf(status_message, "%s", "");
        TransferJob *job = new TransferJob();
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(job->files));
        else
            job->files.push_back(selected_remote_file);
        job->target = local_directory;
        int res = pthread_create(&bk_activity_thid, NULL, DownloadFilesThread, job);
        if (res != 0)
        {
            delete job;
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
            selected_action = ACTION_REFRESH_LOCAL_FILES;
        }
    }

    static void CollectRemoteInstallFiles(RemoteClient *client, const DirEntry &entry,
                                          std::vector<DirEntry> *out, int depth = 0);

    void *InstallRemotePkgsThread(void *argp)
    {
        Activities::Guard activity(ACTIVITY_INSTALL, STR_INSTALLING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        int failed = 0;
        int success = 0;
        int skipped = 0;

        std::vector<DirEntry> selected;
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(selected));
        else
            selected.push_back(selected_remote_file);

        std::vector<DirEntry> files;
        for (size_t i = 0; i < selected.size(); i++)
            CollectRemoteInstallFiles(remoteclient, selected[i], &files);

        for (size_t index = 0; index < files.size(); index++)
        {
            DirEntry &file = files[index];
            if (Activities::Stopped())
                break;
            Activities::CountCurrent((uint32_t)(index + 1), (uint32_t) files.size());
            sprintf(activity_message, "%s %s", lang_strings[STR_INSTALLING], file.name);

            if (!file.isDir)
            {
                std::string path = std::string(file.path);
                path = Util::ToLower(path);
                if (path.size() > 4 && path.substr(path.size() - 4) == ".pkg")
                {
                    pkg_header header;
                    memset(&header, 0, sizeof(header));

                    if (remoteclient->Head(file.path, (void *)&header, sizeof(header)) == 0)
                        failed++;
                    else
                    {
                        if (BE32(header.pkg_magic) == PKG_MAGIC)
                        {
                            Activities::ItemCurrent(file.path);
                            if (!remote_settings->enable_rpi)
                            {
                                if (DownloadAndInstallPkg(file.path, &header) == 0)
                                    failed++;
                                else
                                    success++;
                            }
                            else
                            {
                                bool can_install_via_rpi = INSTALLER::CanInstallViaRPI(file.path, &header);
                                if (!can_install_via_rpi)
                                {
                                    if (DownloadAndInstallPkg(file.path, &header) == 0)
                                        failed++;
                                    else
                                        success++;
                                }
                                else
                                {
                                    if (remote_settings->enable_disk_cache)
                                    {
                                        SplitPkgInstallData *install_data = (SplitPkgInstallData*) malloc(sizeof(SplitPkgInstallData));
                                        memset(install_data, 0, sizeof(SplitPkgInstallData));

                                        OrbisTick tick;
                                        sceRtcGetCurrentTick(&tick);
                                        std::string install_pkg_path = std::string(temp_folder) + "/" + std::to_string(tick.mytick) + ".pkg";
                                        SplitFile *sp = new SplitFile(install_pkg_path, INSTALL_ARCHIVE_PKG_SPLIT_SIZE/2, INSTALL_ARCHIVE_PKG_WRITE_THROTTLE_DISTANCE);

                                        install_data->split_file = sp;
                                        install_data->remote_client = INSTALLER::GetRemoteClient(remote_settings);
                                        if (install_data->remote_client == nullptr)
                                        {
                                            /* No usable client for this site: fail instead of
                                             * handing a NULL client to the download thread */
                                            sprintf(status_message, "%s %s", lang_strings[STR_INSTALL_FAILED], file.name);
                                            delete sp;
                                            free(install_data);
                                            failed++;
                                        }
                                        else
                                        {
                                            install_data->path = file.path;
                                            remoteclient->Size(file.path, &install_data->size);
                                            install_data->stop_write_thread = false;
                                            /* The Google Drive client is shared with the UI, the
                                             * download thread must not delete it */
                                            install_data->delete_client = (install_data->remote_client != remoteclient);

                                            int ret = pthread_create(&install_data->thread, NULL, DownloadSplitPkg, install_data);

                                            if (INSTALLER::InstallSplitPkg(file.path, install_data, false) == 0)
                                                failed++;
                                            else
                                                success++;
                                        }
                                    }
                                    else
                                    {
                                        uint64_t file_size;
                                        remoteclient->Size(file.path, &file_size);
                                        std::string url = INSTALLER::getRemoteUrl(file.path, file_size, true);
                                        std::string title = INSTALLER::GetRemotePkgTitle(remoteclient, file.path, &header);
                                        if (INSTALLER::InstallRemotePkg(url, &header, title) == 0)
                                            failed++;
                                        else
                                            success++;

                                        if (index + 1 < files.size())
                                        {
                                            sleep(1);
                                        }
                                    }
                                }
                            }
                        }
                        else
                            skipped++;
                    }
                }
                else if (Util::EndsWith(path,".zip") || Util::EndsWith(path,".rar") || Util::EndsWith(path,".7z") ||
                        Util::EndsWith(path,".tar.xz") || Util::EndsWith(path,".tar.gz"))
                {
                    ArchiveEntry *entry = ZipUtil::GetPackageEntry(file.path, remoteclient);
                    if (entry != nullptr)
                    {
                        while (entry != nullptr)
                        {
                            Activities::ItemCurrent(entry->filename);
                            snprintf(activity_message, 1023, "%s %s", lang_strings[STR_INSTALLING], entry->filename.c_str());

                            ArchivePkgInstallData *install_data = (ArchivePkgInstallData*) malloc(sizeof(ArchivePkgInstallData));
                            memset(install_data, 0, sizeof(ArchivePkgInstallData));

                            std::string install_pkg_path = std::string(temp_folder) + "/" + entry->filename;
                            SplitFile *sp = new SplitFile(install_pkg_path, INSTALL_ARCHIVE_PKG_SPLIT_SIZE, INSTALL_ARCHIVE_PKG_WRITE_THROTTLE_DISTANCE);
                            
                            install_data->archive_entry = entry;
                            install_data->split_file = sp;
                            install_data->stop_write_thread = false;

                            int res = pthread_create(&install_data->thread, NULL, ExtractArchivePkg, install_data);

                            INSTALLER::InstallArchivePkg(entry->filename, install_data);

                            ArchiveEntry *previos = entry;
                            entry = ZipUtil::GetNextPackageEntry(entry);
                            free(previos);
                        }
                        success++;
                    }
                    else
                        skipped++;
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
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *ExtractArchivePkg(void *argp)
    {
        ssize_t len;
        char *buffer = (char*) malloc(ARCHIVE_TRANSFER_SIZE);

        ArchivePkgInstallData *install_data = (ArchivePkgInstallData*) argp;
        SplitFile *sp = install_data->split_file;

        /* loop over file contents and write to fd */
        sp->Open();
        while (!install_data->stop_write_thread)
        {
            len = archive_read_data(install_data->archive_entry->archive, buffer, ARCHIVE_TRANSFER_SIZE);

            if (len == 0)
                break;

            if (len < 0)
            {
                sprintf(status_message, "error archive_read_data('%s')", install_data->archive_entry->filename.c_str());
                break;
            }

            if (sp->Write(buffer, len) != len)
            {
                sprintf(status_message, "error write('%s')", install_data->archive_entry->filename.c_str());
                break;;
            }
        }

        sp->Close();
        free(buffer);
        return NULL;
    }

    void *DownloadSplitPkg(void *argp)
    {
        SplitPkgInstallData *install_data = (SplitPkgInstallData*) argp;
        SplitFile *sp = install_data->split_file;

        /* loop over file contents and write to fd */
        sp->Open();
        install_data->remote_client->Get(sp, install_data->path);
        sp->Close();
        return NULL;
    }

    static bool IsInstallableLocalFile(const std::string &path)
    {
        std::string lower = Util::ToLower(path);
        return Util::EndsWith(lower, ".pkg") ||
               Util::EndsWith(lower, ".zip") ||
               Util::EndsWith(lower, ".rar") ||
               Util::EndsWith(lower, ".7z") ||
               Util::EndsWith(lower, ".tar.xz") ||
               Util::EndsWith(lower, ".tar.gz") ||
               Util::EndsWith(lower, ".tar.bz2");
    }

    // Walk selected folders first so every installable file is discovered
    // before the first package is installed.
    static void CollectLocalInstallFiles(const DirEntry &entry, std::vector<DirEntry> *out, int depth = 0)
    {
        if (depth > 4 || Activities::Stopped())
            return;

        if (!entry.isDir)
        {
            if (IsInstallableLocalFile(entry.path))
                out->push_back(entry);
            return;
        }

        int err = 0;
        std::vector<DirEntry> entries = FS::ListDir(entry.path, &err);
        if (err)
            return;

        for (size_t i = 0; i < entries.size(); i++)
        {
            if (strcmp(entries[i].name, "..") == 0)
                continue;
            CollectLocalInstallFiles(entries[i], out, depth + 1);
        }
    }

    static void CollectRemoteInstallFiles(RemoteClient *client, const DirEntry &entry,
                                          std::vector<DirEntry> *out, int depth)
    {
        if (client == nullptr || depth > 64 || Activities::Stopped())
            return;

        if (!entry.isDir)
        {
            if (IsInstallableLocalFile(entry.path))
                out->push_back(entry);
            return;
        }

        std::vector<DirEntry> entries = client->ListDir(entry.path);
        for (size_t i = 0; i < entries.size(); i++)
        {
            if (strcmp(entries[i].name, "..") == 0)
                continue;
            CollectRemoteInstallFiles(client, entries[i], out, depth + 1);
        }
    }

    void *InstallLocalPkgsThread(void *argp)
    {
        Activities::Guard activity(ACTIVITY_INSTALL, STR_INSTALLING, ACTIVITY_NATIVE, ACTIVITY_LOCAL);
        int failed = 0;
        int success = 0;
        int skipped = 0;
        int ret;

        std::vector<DirEntry> selected;
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(selected));
        else
            selected.push_back(selected_local_file);

        std::vector<DirEntry> files;
        for (size_t i = 0; i < selected.size(); i++)
            CollectLocalInstallFiles(selected[i], &files);

        for (size_t index = 0; index < files.size(); index++)
        {
            DirEntry &file = files[index];
            if (Activities::Stopped())
                break;
            Activities::CountCurrent((uint32_t)(index + 1), (uint32_t) files.size());
            sprintf(activity_message, "%s %s", lang_strings[STR_INSTALLING], file.name);

            if (!file.isDir)
            {
                std::string path = std::string(file.path);
                path = Util::ToLower(path);
                if (Util::EndsWith(path,".pkg"))
                {
                    pkg_header header;
                    memset(&header, 0, sizeof(header));
                    if (FS::Head(file.path, (void *)&header, sizeof(header)) == 0)
                        failed++;
                    else
                    {
                        if (BE32(header.pkg_magic) == PKG_MAGIC)
                        {
                            Activities::ItemCurrent(file.path);
                            if ((ret = INSTALLER::InstallLocalPkg(file.path, &header)) <= 0)
                            {
                                if (ret == -1)
                                {
                                    sprintf(activity_message, "%s - %s", file.name, lang_strings[STR_INSTALL_FROM_DATA_MSG]);
                                    sceKernelUsleep(3000000);
                                }
                                else if (ret == -2)
                                {
                                    sprintf(activity_message, "%s - %s", file.name, lang_strings[STR_ALREADY_INSTALLED_MSG]);
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
                else if (Util::EndsWith(path,".zip") || Util::EndsWith(path,".rar") || Util::EndsWith(path,".7z") ||
                         Util::EndsWith(path,".tar.xz") || Util::EndsWith(path,".tar.gz") || Util::EndsWith(path,".tar.bz2") )
                {
                    ArchiveEntry *entry = ZipUtil::GetPackageEntry(file.path);
                    if (entry != nullptr)
                    {
                        while (entry != nullptr)
                        {
                            Activities::ItemCurrent(entry->filename);
                            ArchivePkgInstallData *install_data = (ArchivePkgInstallData*) malloc(sizeof(ArchivePkgInstallData));
                            memset(install_data, 0, sizeof(ArchivePkgInstallData));

                            std::string install_pkg_path = std::string(temp_folder) + "/" + entry->filename;
                            SplitFile *sp = new SplitFile(install_pkg_path, INSTALL_ARCHIVE_PKG_SPLIT_SIZE, INSTALL_ARCHIVE_PKG_WRITE_THROTTLE_DISTANCE);
                            
                            install_data->archive_entry = entry;
                            install_data->split_file = sp;
                            install_data->stop_write_thread = false;

                            int res = pthread_create(&install_data->thread, NULL, ExtractArchivePkg, install_data);

                            INSTALLER::InstallArchivePkg(entry->filename, install_data);

                            ArchiveEntry *previous = entry;
                            entry = ZipUtil::GetNextPackageEntry(entry);
                            free(previous);
                        }
                        success++;
                    }
                    else
                        skipped++;
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
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *ExtractZipThread(void *argp)
    {
        ArchiveJob *job = static_cast<ArchiveJob *>(argp);
        Activities::Guard activity(ACTIVITY_EXTRACT, STR_EXTRACTING, ACTIVITY_NATIVE, ACTIVITY_LOCAL);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        FS::MkDirs(target.c_str());
        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;
            if (!it->isDir)
            {
                Activities::ItemCurrent("\"" + std::string(it->path) + "\" to \"" + target + "\"");
                int ret = ZipUtil::Extract(*it, target.c_str());
                if (ret == 0)
                {
                    sprintf(status_message, "%s %s", lang_strings[STR_FAILED_TO_EXTRACT], it->name);
                    sceKernelUsleep(100000);
                }
            }
        }
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void ExtractLocalZips()
    {
        sprintf(status_message, "%s", "");
        ArchiveJob *job = new ArchiveJob();
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(job->files));
        else
            job->files.push_back(selected_local_file);
        job->target = extract_zip_folder;

        int res = pthread_create(&bk_activity_thid, NULL, ExtractZipThread, job);
        if (res != 0)
        {
            delete job;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *ExtractRemoteZipThread(void *argp)
    {
        ArchiveJob *job = static_cast<ArchiveJob *>(argp);
        Activities::Guard activity(ACTIVITY_EXTRACT, STR_EXTRACTING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        FS::MkDirs(target.c_str());
        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;
            if (!it->isDir)
            {
                Activities::ItemCurrent("\"" + std::string(it->path) + "\" to \"" + target + "\"");
                int ret = ZipUtil::Extract(*it, target.c_str(), remoteclient);
                if (ret == 0)
                {
                    sprintf(status_message, "%s %s", lang_strings[STR_FAILED_TO_EXTRACT], it->name);
                    sceKernelUsleep(100000);
                }
            }
        }
        multi_selected_remote_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void ExtractRemoteZips()
    {
        sprintf(status_message, "%s", "");
        ArchiveJob *job = new ArchiveJob();
        if (multi_selected_remote_files.size() > 0)
            std::copy(multi_selected_remote_files.begin(), multi_selected_remote_files.end(), std::back_inserter(job->files));
        else
            job->files.push_back(selected_remote_file);
        job->target = extract_zip_folder;

        int res = pthread_create(&bk_activity_thid, NULL, ExtractRemoteZipThread, job);
        if (res != 0)
        {
            delete job;
            multi_selected_remote_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *MakeZipThread(void *argp)
    {
        ArchiveJob *job = static_cast<ArchiveJob *>(argp);
        Activities::Guard activity(ACTIVITY_COMPRESS, STR_COMPRESSING, ACTIVITY_NATIVE, ACTIVITY_LOCAL);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        zipFile zf = zipOpen64(target.c_str(), APPEND_STATUS_CREATE);
        if (zf != NULL)
        {
            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
            {
                if (Activities::Stopped())
                    break;

                Activities::ItemCurrent("\"" + std::string(it->path) + "\" to \"" + target + "\"");
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
        multi_selected_local_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void MakeLocalZip()
    {
        sprintf(status_message, "%s", "");
        ArchiveJob *job = new ArchiveJob();
        if (multi_selected_local_files.size() > 0)
            std::copy(multi_selected_local_files.begin(), multi_selected_local_files.end(), std::back_inserter(job->files));
        else
            job->files.push_back(selected_local_file);
        job->target = zip_file_path;

        int res = pthread_create(&bk_activity_thid, NULL, MakeZipThread, job);
        if (res != 0)
        {
            delete job;
            multi_selected_local_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *InstallLocalUrlPkgThread(void *argp)
    {
        Activities::Guard activity(ACTIVITY_INSTALL, STR_INSTALLING, ACTIVITY_NATIVE, ACTIVITY_LOCAL);
        sprintf(status_message, "%s", "");
        pkg_header header;
        char filename[2000];
        OrbisDateTime now;
        OrbisTick tick;
        sceRtcGetCurrentClockLocalTime(&now);
        sceRtcGetTick(&now, &tick);
        sprintf(filename, "%s/%lu.pkg", TMP_FOLDER_PATH, tick.mytick);

        std::string full_url = std::string(install_pkg_url.url);
        FileHost *filehost = FileHost::getFileHost(full_url, install_pkg_url.enable_alldebrid, install_pkg_url.enable_realdebrid);
        if (!filehost->IsValidUrl())
        {
            sprintf(status_message, "%s", lang_strings[STR_FAIL_TO_OBTAIN_GG_DL_MSG]);
            Windows::SetModalMode(false);
            return NULL;
        }
        full_url = filehost->GetDownloadUrl();
        delete(filehost);

        if (full_url.empty())
        {
            sprintf(status_message, "%s", lang_strings[STR_FAIL_TO_OBTAIN_GG_DL_MSG]);
            Windows::SetModalMode(false);
            return NULL;
        }
        size_t scheme_pos = full_url.find_first_of("://");
        size_t path_pos = full_url.find_first_of("/", scheme_pos + 3);
        std::string host = full_url.substr(0, path_pos);
        std::string path = full_url.substr(path_pos);

        BaseClient tmp_client;
        tmp_client.Connect(host.c_str(), install_pkg_url.username, install_pkg_url.password);

        sprintf(activity_message, "%s URL to %s", lang_strings[STR_DOWNLOADING], filename);
        int s = sizeof(pkg_header);
        memset(&header, 0, s);

        uint64_t total_size = 0;
        int ret = tmp_client.Size(path, &total_size);
        if (ret == 0)
        {
            sprintf(status_message, "%s", tmp_client.LastResponse());
            tmp_client.Quit();
            Windows::SetModalMode(false);
            return NULL;
        }
        tmp_client.SetTotalBytes(total_size);

        int is_performed = tmp_client.Get(filename, path);

        if (is_performed == 0)
        {
            sprintf(status_message, "%s - %s", lang_strings[STR_FAILED], tmp_client.LastResponse());
            tmp_client.Quit();
            Windows::SetModalMode(false);
            return NULL;
        }
        tmp_client.Quit();

        FILE *in = FS::OpenRead(filename);
        if (in == NULL)
        {
            sprintf(status_message, "%s - Error opening temp pkg file - %s", lang_strings[STR_FAILED], filename);
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

        Windows::SetModalMode(false);
        return NULL;
    }

    void *InstallRpiUrlPkgThread(void *argp)
    {
        Activities::Guard activity(ACTIVITY_INSTALL, STR_INSTALLING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        json_object *params = json_object_new_object();
        JsonObjectRef params_ref(params);
        json_object_object_add(params, "url", json_object_new_string(install_pkg_url.url));
        json_object_object_add(params, "use_alldebrid", json_object_new_boolean(install_pkg_url.enable_alldebrid));
        json_object_object_add(params, "use_realdebrid", json_object_new_boolean(install_pkg_url.enable_realdebrid));
        json_object_object_add(params, "use_disk_cache", json_object_new_boolean(install_pkg_url.enable_disk_cache));

        const char *params_str = json_object_to_json_string(params);

        char host[128];
        sprintf(host, "http://127.0.0.1:%d", http_server_port);

        httplib::Client tmp_client(host);
        tmp_client.set_keep_alive(true);
        tmp_client.set_follow_location(true);
        tmp_client.set_connection_timeout(30);
        tmp_client.set_read_timeout(30);
        tmp_client.enable_server_certificate_verification(false);

        auto res = tmp_client.Post("/__local__/install_url", params_str, strlen(params_str), "application/json");
        if (res != nullptr && HTTP_SUCCESS(res->status))
        {
            json_object *jobj = json_tokener_parse(res->body.c_str());
            JsonObjectRef jobj_ref(jobj);
            if (jobj != nullptr)
            {
                json_object *result = json_object_object_get(jobj, "result");
                if (result != nullptr)
                {
                    bool success = json_object_get_boolean(json_object_object_get(result, "success"));
                    if (!success)
                    {
                        std::string error_message = JsonGetString(result, "error");
                        sprintf(status_message, "%s", error_message.c_str());
                        Windows::SetModalMode(false);
                    }
                }
            }
            else
            {
                Windows::SetModalMode(false);
            }
        }
        else
        {
            Windows::SetModalMode(false);
        }

        return NULL;
    }

    void InstallUrlPkg()
    {
        int res;
        sprintf(status_message, "%s", "");

        if (!install_pkg_url.enable_rpi)
            res = pthread_create(&bk_activity_thid, NULL, InstallLocalUrlPkgThread, NULL);
        else
            res = pthread_create(&bk_activity_thid, NULL, InstallRpiUrlPkgThread, NULL);

        if (res != 0)
        {
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
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_RCLONE) == 0)
                remoteclient = new RCloneClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_ARCHIVEORG) == 0)
                remoteclient = new ArchiveOrgClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_MYRIENT) == 0)
                remoteclient = new MyrientClient();
            else if (strcmp(remote_settings->http_server_type, HTTP_SERVER_GITHUB) == 0)
                remoteclient = new GithubClient();
        }
        else if (strncmp(remote_settings->server, "webdavs://", 10) == 0 || strncmp(remote_settings->server, "webdav://", 9) == 0)
        {
            remoteclient = new WebDAVClient();
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
        else if (strncmp(remote_settings->server, "sftp://", 7) == 0)
        {
            remoteclient = new SFTPClient();
        }
        else if (strncmp(remote_settings->server, "nfs://", 6) == 0)
        {
            remoteclient = new NfsClient();
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
        if (Activities::Stopped())
            return 0;
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && FS::FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest);
            confirm_kind = CONFIRM_KIND_OVERWRITE;
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            selected_action = action_to_take;
            if (confirm_state == CONFIRM_ALL)
            {
                dont_prompt_overwrite = true;
                confirm_state = CONFIRM_YES;
            }
            else if (confirm_state == CONFIRM_CANCEL)
            {
                Activities::Cancel(Activities::CurrentId());
                confirm_state = CONFIRM_NO;
            }
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
                return FS::Copy(src, dest, []() { return Activities::Stopped(); });
            else
                return FS::Move(src, dest, []() { return Activities::Stopped(); });
        }

        return 1;
    }

    int CopyOrMove(const DirEntry &src, const char *dest, bool isCopy)
    {
        if (Activities::Stopped())
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            if (!isCopy && !FS::FolderExists(dest))
            {
                errno = 0;
                int ret = rename(src.path, dest);
                if (ret != 0 && errno != EXDEV && errno != EEXIST)
                {
                    return 0;
                }
            }

            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            FS::MkDirs(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (Activities::Stopped())
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                    {
                        free(new_path);
                        continue;
                    }

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
                    remoteclient->SetTotalBytes(entries[i].file_size);
                    remoteclient->ResetTransferredBytes();
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

            if (!isCopy && FS::FolderExists(src.path))
            {
                FS::RmDir(src.path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "%s %s", isCopy ? lang_strings[STR_COPYING] : lang_strings[STR_MOVING], src.name);
            remoteclient->SetTotalBytes(src.file_size);
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
        TransferJob *job = static_cast<TransferJob *>(argp);
        Activities::Guard activity(ACTIVITY_MOVE, STR_MOVING, ACTIVITY_NATIVE, ACTIVITY_LOCAL);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;

            if (strcmp(it->directory, target.c_str()) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", target.c_str(), FS::hasEndSlash(target.c_str()) ? "" : "/", it->name);

            if (it->isDir)
            {
                if (strncmp(target.c_str(), it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
                CopyOrMove(*it, new_path, false);
            }
            else
            {
                CopyOrMove(*it, new_path, false);
            }
        }
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void MoveLocalFiles()
    {
        sprintf(status_message, "%s", "");
        TransferJob *job = new TransferJob();
        job->files.assign(local_paste_files.begin(), local_paste_files.end());
        job->target = local_directory;
        int res = pthread_create(&bk_activity_thid, NULL, MoveLocalFilesThread, job);
        if (res != 0)
        {
            delete job;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    void *CopyLocalFilesThread(void *argp)
    {
        TransferJob *job = static_cast<TransferJob *>(argp);
        Activities::Guard activity(ACTIVITY_COPY, STR_COPYING, ACTIVITY_NATIVE, ACTIVITY_LOCAL);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;

            if (strcmp(it->directory, target.c_str()) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", target.c_str(), FS::hasEndSlash(target.c_str()) ? "" : "/", it->name);

            if (it->isDir)
            {
                if (strncmp(target.c_str(), it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_COPY_TO_SUBDIR_MSG]);
                    continue;
                }
                CopyOrMove(*it, new_path, true);
            }
            else
            {
                CopyOrMove(*it, new_path, true);
            }
        }
        local_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return NULL;
    }

    void CopyLocalFiles()
    {
        sprintf(status_message, "%s", "");
        TransferJob *job = new TransferJob();
        job->files.assign(local_paste_files.begin(), local_paste_files.end());
        job->target = local_directory;
        int res = pthread_create(&bk_activity_thid, NULL, CopyLocalFilesThread, job);
        if (res != 0)
        {
            delete job;
            local_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyOrMoveRemoteFile(const std::string &src, const std::string &dest, bool isCopy)
    {
        if (Activities::Stopped())
            return 0;
        int ret;
        if (overwrite_type == OVERWRITE_PROMPT && remoteclient->FileExists(dest))
        {
            sprintf(confirm_message, "%s %s?", lang_strings[STR_OVERWRITE], dest.c_str());
            confirm_kind = CONFIRM_KIND_OVERWRITE;
            confirm_state = CONFIRM_WAIT;
            action_to_take = selected_action;
            while (confirm_state == CONFIRM_WAIT)
            {
                sceKernelUsleep(100000);
            }
            selected_action = action_to_take;
            if (confirm_state == CONFIRM_ALL)
            {
                dont_prompt_overwrite = true;
                confirm_state = CONFIRM_YES;
            }
            else if (confirm_state == CONFIRM_CANCEL)
            {
                Activities::Cancel(Activities::CurrentId());
                confirm_state = CONFIRM_NO;
            }
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
        TransferJob *job = static_cast<TransferJob *>(argp);
        Activities::Guard activity(ACTIVITY_MOVE, STR_MOVING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;

            if (strcmp(it->directory, target.c_str()) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", target.c_str(), FS::hasEndSlash(target.c_str()) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(target.c_str(), it->path, strlen(it->path)) == 0)
                {
                    sprintf(status_message, "%s", lang_strings[STR_CANT_MOVE_TO_SUBDIR_MSG]);
                    continue;
                }
                int res = CopyOrMoveRemoteFile(it->path, new_path, false);
                if (res == 0)
                    sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
            else
            {
                int res = CopyOrMoveRemoteFile(it->path, new_path, false);
                if (res == 0)
                    sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
        }
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void MoveRemoteFiles()
    {
        sprintf(status_message, "%s", "");
        TransferJob *job = new TransferJob();
        job->files.assign(remote_paste_files.begin(), remote_paste_files.end());
        job->target = remote_directory;
        int res = pthread_create(&bk_activity_thid, NULL, MoveRemoteFilesThread, job);
        if (res != 0)
        {
            delete job;
            remote_paste_files.clear();
            Windows::SetModalMode(false);
        }
    }

    int CopyRemotePath(const DirEntry &src, const char *dest)
    {
        if (Activities::Stopped())
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = remoteclient->ListDir(src.path);
            remoteclient->Mkdir(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                if (Activities::Stopped())
                    return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    remoteclient->Mkdir(new_path);
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
                    remoteclient->SetTotalBytes(entries[i].file_size);
                    remoteclient->ResetTransferredBytes();
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
        TransferJob *job = static_cast<TransferJob *>(argp);
        Activities::Guard activity(ACTIVITY_COPY, STR_COPYING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        std::vector<DirEntry> files = job->files;
        std::string target = job->target;
        delete job;

        for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end(); ++it)
        {
            if (Activities::Stopped())
                break;

            if (strcmp(it->directory, target.c_str()) == 0)
                continue;

            char new_path[1024];
            sprintf(new_path, "%s%s%s", target.c_str(), FS::hasEndSlash(target.c_str()) ? "" : "/", it->name);
            if (it->isDir)
            {
                if (strncmp(target.c_str(), it->path, strlen(it->path)) == 0)
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
                int res = CopyRemotePath(*it, new_path);
                if (res == 0)
                    sprintf(status_message, "%s - %s", it->name, lang_strings[STR_FAIL_COPY_MSG]);
            }
        }
        remote_paste_files.clear();
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return NULL;
    }

    void CopyRemoteFiles()
    {
        sprintf(status_message, "%s", "");
        TransferJob *job = new TransferJob();
        job->files.assign(remote_paste_files.begin(), remote_paste_files.end());
        job->target = remote_directory;
        int res = pthread_create(&bk_activity_thid, NULL, CopyRemoteFilesThread, job);
        if (res != 0)
        {
            delete job;
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
        sprintf(local_file, "%s/%lu.pkg", temp_folder, tick.mytick);

        sprintf(activity_message, "%s %s to %s", lang_strings[STR_DOWNLOADING], filename.c_str(), local_file);
        uint64_t total_size = 0;
        remoteclient->Size(filename, &total_size);
        remoteclient->SetTotalBytes(total_size);
        remoteclient->ResetTransferredBytes();
        int ret = remoteclient->Get(local_file, filename);
        if (ret == 0)
            return 0;

        return INSTALLER::InstallLocalPkg(local_file, header, true);
    }

    void *DownloadAndInstallPkgThread(void *argp)
    {
        Activities::Guard activity(ACTIVITY_INSTALL, STR_INSTALLING, ACTIVITY_NATIVE, ACTIVITY_REMOTE);
        DownloadPkgData* pkg_data = (DownloadPkgData*) argp;

        int ret;
        char local_file[2000];
        OrbisDateTime now;
        OrbisTick tick;
        sceRtcGetCurrentClockLocalTime(&now);
        sceRtcGetTick(&now, &tick);
        sprintf(local_file, "%s/%lu.pkg", temp_folder, tick.mytick);

        sprintf(activity_message, "%s %s to %s", lang_strings[STR_DOWNLOADING], pkg_data->path, local_file);
        uint64_t total_size = 0;
        pkg_data->client->Size(pkg_data->path, &total_size);
        pkg_data->client->SetTotalBytes(total_size);
        pkg_data->client->ResetTransferredBytes();

        ret = pkg_data->client->Get(local_file, pkg_data->path);
        if (ret == 0 || Activities::Stopped())
        {
            FS::Rm(local_file);
            delete pkg_data->client;
            free(pkg_data->header);
            free(pkg_data->path);
            free(pkg_data);

            Windows::SetModalMode(false);
            return NULL;
        }

        ret = INSTALLER::InstallLocalPkg(local_file, pkg_data->header, true);
        if (ret == 0)
            FS::Rm(local_file);

        delete pkg_data->client;
        free(pkg_data->header);
        free(pkg_data->path);
        free(pkg_data);
        Windows::SetModalMode(false);

        return NULL;
    }

    int DownloadAndInstallPkg(RemoteClient *client, const std::string &path, pkg_header *header)
    {
        sprintf(status_message, "%s", "");
        DownloadPkgData *pkg_data = (DownloadPkgData*) malloc(sizeof(DownloadPkgData));
        pkg_data->header = (pkg_header*) malloc(sizeof(pkg_header));
        pkg_data->path = (char*) malloc(2048);
        pkg_data->client = client;
        memcpy(pkg_data->header, header, sizeof(pkg_header));
        snprintf(pkg_data->path, 2047, "%s", path.c_str());

        int res = pthread_create(&bk_activity_thid, NULL, DownloadAndInstallPkgThread, pkg_data);
        if (res != 0)
        {
            remote_paste_files.clear();
            Windows::SetModalMode(false);
            return 0;
        }
        return 1;
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

    void RestartServer()
    {
        INSTALLER::StopEzRemoteServer();
        sleep(2);
        INSTALLER::StartEzRemoteServer();
        sleep(2);
    }

    void GetBackgroundDownloadProgress()
    {
        httplib::Client client = httplib::Client("http://127.0.0.1:" + std::to_string(http_int_server_port));

        if (auto res = client.Get("/get_download_state"))
        {
            if (HTTP_SUCCESS(res->status))
            {
                bg_download_progress.clear();

                json_object *jobj = json_tokener_parse(res->body.c_str());
                JsonObjectRef jobj_ref(jobj);
                if (jobj != nullptr && json_object_get_type(jobj) == json_type_array)
                {
                    struct array_list *progress_list = json_object_get_array(jobj);

                    for (size_t idx = 0; idx < progress_list->length; ++idx)
                    {
                        DownloadProgress progress;

                        json_object *progress_obj = (json_object *)array_list_get_idx(progress_list, idx);
                        progress.path = JsonGetString(progress_obj, "path");
                        progress.bytes_transfered = json_object_get_uint64(json_object_object_get(progress_obj, "bytes_transfered"));
                        progress.file_size = json_object_get_uint64(json_object_object_get(progress_obj, "file_size"));
                        progress.state = state_strings[json_object_get_int(json_object_object_get(progress_obj, "state"))];
                        progress.timestamp = json_object_get_uint64(json_object_object_get(progress_obj, "timestamp"));

                        bg_download_progress.push_back(progress);
                    }
                }
            }
            else
            {
                snprintf(status_message, 1024, "Failed to get the download progress from ezRemote Server");
            }
        }
    }

    RemoteClient *CreateRemoteClient(RemoteSettings *settings)
    {
        if (strncmp(settings->server, GOOGLE_DRIVE_BASE_URL, strlen(GOOGLE_DRIVE_BASE_URL)) == 0)
            return new GDriveClient();

        if (strncmp(settings->server, "https://", 8) == 0 || strncmp(settings->server, "http://", 7) == 0)
        {
            if (strcmp(settings->http_server_type, HTTP_SERVER_APACHE) == 0)
                return new ApacheClient();
            if (strcmp(settings->http_server_type, HTTP_SERVER_MS_IIS) == 0)
                return new IISClient();
            if (strcmp(settings->http_server_type, HTTP_SERVER_NGINX) == 0)
                return new NginxClient();
            if (strcmp(settings->http_server_type, HTTP_SERVER_NPX_SERVE) == 0)
                return new NpxServeClient();
            if (strcmp(settings->http_server_type, HTTP_SERVER_RCLONE) == 0)
                return new RCloneClient();
            if (strcmp(settings->http_server_type, HTTP_SERVER_ARCHIVEORG) == 0)
                return new ArchiveOrgClient();
            if (strcmp(settings->http_server_type, HTTP_SERVER_MYRIENT) == 0)
                return new MyrientClient();
            if (strcmp(settings->http_server_type, HTTP_SERVER_GITHUB) == 0)
                return new GithubClient();
            return nullptr;
        }

        if (strncmp(settings->server, "webdavs://", 10) == 0 || strncmp(settings->server, "webdav://", 9) == 0)
            return new WebDAVClient();

        if (strncmp(settings->server, "smb://", 6) == 0)
            return new SmbClient();

        if (strncmp(settings->server, "ftp://", 6) == 0)
        {
            FtpClient *client = new FtpClient();
            client->SetConnmode(FtpClient::pasv);
            client->SetCallbackBytes(1);
            client->SetCallbackXferFunction(FtpCallback);
            return client;
        }

        if (strncmp(settings->server, "sftp://", 7) == 0)
            return new SFTPClient();

        if (strncmp(settings->server, "nfs://", 6) == 0)
            return new NfsClient();

        return nullptr;
    }

    static bool LocalPathExists(const std::string &path)
    {
        return FS::FileExists(path.c_str()) || FS::IsFolder(path.c_str()) == 1;
    }

    std::string UniqueLocalName(const std::string &dir, const std::string &name)
    {
        std::string sep = FS::hasEndSlash(dir.c_str()) ? "" : "/";
        if (!LocalPathExists(dir + sep + name))
            return name;

        size_t dot = name.find_last_of('.');
        std::string base = (dot == std::string::npos || dot == 0) ? name : name.substr(0, dot);
        std::string ext = (dot == std::string::npos || dot == 0) ? "" : name.substr(dot);

        for (int index = 2; index < 10000; index++)
        {
            char candidate[512];
            snprintf(candidate, sizeof(candidate), "%s (%d)%s", base.c_str(), index, ext.c_str());
            if (!LocalPathExists(dir + sep + candidate))
                return std::string(candidate);
        }

        return name;
    }

    bool DeleteRemoteTree(RemoteClient *client, const std::string &path, int depth)
    {
        if (Activities::Stopped())
            return false;

        if (depth > 64)
            return false;

        if (client->RecursiveDeleteIsAtomic())
            return client->Rmdir(path, true);

        std::vector<DirEntry> entries = client->ListDir(path);
        for (size_t i = 0; i < entries.size(); i++)
        {
            if (!entries[i].selectable)
                continue;

            if (strcmp(entries[i].name, ".") == 0 || strcmp(entries[i].name, "..") == 0)
                continue;

            if (Activities::Stopped())
                return false;

            if (entries[i].isDir)
            {
                if (!DeleteRemoteTree(client, entries[i].path, depth + 1))
                    return false;
            }
            else
            {
                snprintf(activity_message, 1024, "%s %s", lang_strings[STR_DELETING], entries[i].path);
                Activities::ItemCurrent(entries[i].path);
                if (!client->Delete(entries[i].path))
                    return false;
            }
        }

        return client->Rmdir(path, false);
    }

    int DownloadAndInstallPkg(const std::string &filename, pkg_header *header, RemoteClient *client)
    {
        return DownloadAndInstallPkg(client, filename, header);
    }

    int InstallPkgByDownload(RemoteClient *client, const std::string &path, pkg_header *header)
    {
        // Synchronous on purpose: the caller owns the client and would delete
        // it as soon as this returns. The async variant transfers ownership to
        // its worker thread, which is not the contract here.
        char local_file[2000];
        snprintf(local_file, sizeof(local_file), "%s/%llu.pkg", temp_folder,
                 (unsigned long long) Util::GetTick());

        snprintf(activity_message, 1024, "%s %s to %s", lang_strings[STR_DOWNLOADING],
                 path.c_str(), local_file);
        uint64_t size = 0;
        client->Size(path, &size);
        client->SetTotalBytes(size);
        client->ResetTransferredBytes();

        if (Activities::Stopped())
        {
            return 0;
        }

        int ret = client->Get(local_file, path);
        if (ret == 0 || Activities::Stopped())
        {
            FS::Rm(local_file);
            return 0;
        }

        // Downloading is cancellable. Once the package is handed to the
        // installer, interrupting it would leave an inconsistent install.
        Activities::SetCancellable(false);
        ret = INSTALLER::InstallLocalPkg(local_file, header, true);
        Activities::SetCancellable(true);
        if (ret == 0)
            FS::Rm(local_file);
        return ret;
    }

    int InstallPkgByStreaming(RemoteClient *stream_client, const std::string &stream_path,
                              const std::string &cache_key)
    {
        SplitPkgInstallData *install_data = (SplitPkgInstallData*) malloc(sizeof(SplitPkgInstallData));
        memset(install_data, 0, sizeof(SplitPkgInstallData));

        std::string install_pkg_path = std::string(temp_folder) + "/" + std::to_string(Util::GetTick()) + ".pkg";
        SplitFile *sp = CreateFileStream(install_pkg_path, INSTALL_ARCHIVE_PKG_SPLIT_SIZE/2);

        install_data->split_file = sp;
        install_data->remote_client = stream_client;
        install_data->path = stream_path;
        stream_client->Size(stream_path, (uint64_t*)&install_data->size);
        install_data->stop_write_thread = false;
        install_data->delete_client = true;

        pthread_create(&install_data->thread, NULL, DownloadSplitPkg, install_data);

        Activities::SetCancellable(false);
        int ret = INSTALLER::InstallSplitPkg(cache_key, install_data, false);
        Activities::SetCancellable(true);

        if (Activities::Stopped())
            FS::Rm(install_pkg_path.c_str());

        return ret;
    }

    int InstallPkgByProxy(const std::string &url, pkg_header *header, const std::string &title)
    {
        Activities::SetCancellable(false);
        int ret = INSTALLER::InstallRemotePkg(url, header, title);
        Activities::SetCancellable(true);
        return ret;
    }

    int InstallRemotePkgFile(RemoteSettings *settings, RemoteClient *client, const std::string &path,
                             pkg_header *header, uint64_t file_size)
    {
        if (!settings->enable_rpi)
            return InstallPkgByDownload(client, path, header);

        if (settings->enable_disk_cache)
            return InstallPkgByStreaming(INSTALLER::GetRemoteClient(settings), path, path);

        std::string url = INSTALLER::getRemoteUrl(settings, client, path, file_size, true);
        std::string title = INSTALLER::GetRemotePkgTitle(client, path, header);
        return InstallPkgByProxy(url, header, title);
    }

    void StopServer()
    {
        httplib::Client tmp_client(std::string("http://127.0.0.1:") + std::to_string(http_int_server_port));
        tmp_client.set_connection_timeout(1);
        tmp_client.Get("/stop");
    }

    int ClearDownloadHistory()
    {
        httplib::Client tmp_client(std::string("http://127.0.0.1:") + std::to_string(http_int_server_port));
        tmp_client.set_connection_timeout(2);
        if (auto res = tmp_client.Post("/clear_download_history", "{}", "application/json"))
        {
            if (HTTP_SUCCESS(res->status))
            {
                bg_download_progress.clear();
                return 1;
            }
        }

        return 0;
    }
}
