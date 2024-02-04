#ifndef ACTIONS_H
#define ACTIONS_H

#include <pthread.h>
#include "installer.h"
#include "common.h"

#define CONFIRM_NONE -1
#define CONFIRM_WAIT 0
#define CONFIRM_YES 1
#define CONFIRM_NO 2

enum ACTIONS
{
    ACTION_NONE = 0,
    ACTION_UPLOAD,
    ACTION_DOWNLOAD,
    ACTION_DELETE_LOCAL,
    ACTION_DELETE_REMOTE,
    ACTION_RENAME_LOCAL,
    ACTION_RENAME_REMOTE,
    ACTION_NEW_LOCAL_FOLDER,
    ACTION_NEW_REMOTE_FOLDER,
    ACTION_CHANGE_LOCAL_DIRECTORY,
    ACTION_CHANGE_REMOTE_DIRECTORY,
    ACTION_APPLY_LOCAL_FILTER,
    ACTION_APPLY_REMOTE_FILTER,
    ACTION_REFRESH_LOCAL_FILES,
    ACTION_REFRESH_REMOTE_FILES,
    ACTION_SHOW_LOCAL_PROPERTIES,
    ACTION_SHOW_REMOTE_PROPERTIES,
    ACTION_LOCAL_SELECT_ALL,
    ACTION_REMOTE_SELECT_ALL,
    ACTION_LOCAL_CLEAR_ALL,
    ACTION_REMOTE_CLEAR_ALL,
    ACTION_CONNECT,
    ACTION_DISCONNECT,
    ACTION_DISCONNECT_AND_EXIT,
    ACTION_INSTALL_REMOTE_PKG,
    ACTION_INSTALL_LOCAL_PKG,
    ACTION_INSTALL_URL_PKG,
    ACTION_EXTRACT_LOCAL_ZIP,
    ACTION_CREATE_LOCAL_ZIP,
    ACTION_LOCAL_CUT,
    ACTION_LOCAL_COPY,
    ACTION_LOCAL_PASTE,
    ACTION_LOCAL_EDIT,
    ACTION_REMOTE_CUT,
    ACTION_REMOTE_COPY,
    ACTION_REMOTE_PASTE,
    ACTION_REMOTE_EDIT,
    ACTION_NEW_LOCAL_FILE,
    ACTION_NEW_REMOTE_FILE,
    ACTION_SET_DEFAULT_LOCAL_FOLDER,
    ACTION_SET_DEFAULT_REMOTE_FOLDER,
    ACTION_VIEW_LOCAL_IMAGE,
    ACTION_VIEW_REMOTE_IMAGE,
    ACTION_VIEW_LOCAL_PKG,
    ACTION_VIEW_REMOTE_PKG,
    ACTION_EXTRACT_REMOTE_ZIP,
};

enum OverWriteType
{
    OVERWRITE_NONE = 0,
    OVERWRITE_PROMPT,
    OVERWRITE_ALL
};

static pthread_t bk_activity_thid;
static pthread_t ftp_keep_alive_thid;

namespace Actions
{

    void RefreshLocalFiles(bool apply_filter);
    void RefreshRemoteFiles(bool apply_filter);
    void HandleChangeLocalDirectory(const DirEntry entry);
    void HandleChangeRemoteDirectory(const DirEntry entry);
    void HandleRefreshLocalFiles();
    void HandleRefreshRemoteFiles();
    void CreateNewLocalFolder(char *new_folder);
    void CreateNewRemoteFolder(char *new_folder);
    void RenameLocalFolder(const char *old_path, const char *new_path);
    void RenameRemoteFolder(const char *old_path, const char *new_path);
    void *DeleteSelectedLocalFilesThread(void *argp);
    void DeleteSelectedLocalFiles();
    void *DeleteSelectedRemotesFilesThread(void *argp);
    void DeleteSelectedRemotesFiles();
    void *UploadFilesThread(void *argp);
    void UploadFiles();
    void *DownloadFilesThread(void *argp);
    void DownloadFiles();
    void Connect();
    void Disconnect();
    void SelectAllLocalFiles();
    void SelectAllRemoteFiles();
    void *InstallRemotePkgsThread(void *argp);
    void InstallRemotePkgs();
    void *InstallLocalPkgsThread(void *argp);
    void InstallLocalPkgs();
    void *InstallUrlPkgThread(void *argp);
    void InstallUrlPkg();
    void *KeepAliveThread(void *argp);
    void *ExtractZipThread(void *argp);
    void ExtractLocalZips();
    void *ExtractRemoteZipThread(void *argp);
    void ExtractRemoteZips();
    void *MakeZipThread(void *argp);
    void MakeLocalZip();
    void *MoveLocalFilesThread(void *argp);
    void MoveLocalFiles();
    void *CopyLocalFilesThread(void *argp);
    void CopyLocalFiles();
    void *MoveRemoteFilesThread(void *argp);
    void MoveRemoteFiles();
    void *CopyRemoteFilesThread(void *argp);
    void CopyRemoteFiles();
    int DownloadAndInstallPkg(const std::string &filename, pkg_header *header);
    void CreateLocalFile(char *filename);
    void CreateRemoteFile(char *filename);
    void *ExtractArchivePkg(void *argp);
}

#endif