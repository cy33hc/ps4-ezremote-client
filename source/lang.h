#ifndef __LANG_H__
#define __LANG_H__

#include "config.h"

#define FOREACH_STR(FUNC)                   \
	FUNC(STR_CONNECTION_SETTINGS)           \
	FUNC(STR_SITE)                          \
	FUNC(STR_LOCAL)                         \
	FUNC(STR_REMOTE)                        \
	FUNC(STR_MESSAGES)                      \
	FUNC(STR_UPDATE_SOFTWARE)               \
	FUNC(STR_CONNECT)                       \
	FUNC(STR_DISCONNECT)                    \
	FUNC(STR_SEARCH)                        \
	FUNC(STR_REFRESH)                       \
	FUNC(STR_SERVER)                        \
	FUNC(STR_USERNAME)                      \
	FUNC(STR_PASSWORD)                      \
	FUNC(STR_PORT)                          \
	FUNC(STR_PASV)                          \
	FUNC(STR_DIRECTORY)                     \
	FUNC(STR_FILTER)                        \
	FUNC(STR_YES)                           \
	FUNC(STR_NO)                            \
	FUNC(STR_CANCEL)                        \
	FUNC(STR_CONTINUE)                      \
	FUNC(STR_CLOSE)                         \
	FUNC(STR_FOLDER)                        \
	FUNC(STR_FILE)                          \
	FUNC(STR_TYPE)                          \
	FUNC(STR_NAME)                          \
	FUNC(STR_SIZE)                          \
	FUNC(STR_DATE)                          \
	FUNC(STR_NEW_FOLDER)                    \
	FUNC(STR_RENAME)                        \
	FUNC(STR_DELETE)                        \
	FUNC(STR_UPLOAD)                        \
	FUNC(STR_DOWNLOAD)                      \
	FUNC(STR_SELECT_ALL)                    \
	FUNC(STR_CLEAR_ALL)                     \
	FUNC(STR_UPLOADING)                     \
	FUNC(STR_DOWNLOADING)                   \
	FUNC(STR_OVERWRITE)                     \
	FUNC(STR_DONT_OVERWRITE)                \
	FUNC(STR_ASK_FOR_CONFIRM)               \
	FUNC(STR_DONT_ASK_CONFIRM)              \
	FUNC(STR_ALLWAYS_USE_OPTION)            \
	FUNC(STR_ACTIONS)                       \
	FUNC(STR_CONFIRM)                       \
	FUNC(STR_OVERWRITE_OPTIONS)             \
	FUNC(STR_PROPERTIES)                    \
	FUNC(STR_PROGRESS)                      \
	FUNC(STR_UPDATES)                       \
	FUNC(STR_DEL_CONFIRM_MSG)               \
	FUNC(STR_CANCEL_ACTION_MSG)             \
	FUNC(STR_FAIL_UPLOAD_MSG)               \
	FUNC(STR_FAIL_DOWNLOAD_MSG)             \
	FUNC(STR_FAIL_READ_LOCAL_DIR_MSG)       \
	FUNC(STR_CONNECTION_CLOSE_ERR_MSG)      \
	FUNC(STR_REMOTE_TERM_CONN_MSG)          \
	FUNC(STR_FAIL_LOGIN_MSG)                \
	FUNC(STR_FAIL_TIMEOUT_MSG)              \
	FUNC(STR_FAIL_DEL_DIR_MSG)              \
	FUNC(STR_DELETING)                      \
	FUNC(STR_FAIL_DEL_FILE_MSG)             \
	FUNC(STR_DELETED)                       \
	FUNC(STR_LINK)                          \
	FUNC(STR_SHARE)                         \
	FUNC(STR_FAILED)                        \
	FUNC(STR_FAIL_CREATE_LOCAL_FILE_MSG)    \
	FUNC(STR_INSTALL)                       \
	FUNC(STR_INSTALLING)                    \
	FUNC(STR_INSTALL_SUCCESS)               \
	FUNC(STR_INSTALL_FAILED)                \
	FUNC(STR_INSTALL_SKIPPED)               \
	FUNC(STR_CHECK_HTTP_MSG)                \
	FUNC(STR_FAILED_HTTP_CHECK)             \
	FUNC(STR_REMOTE_NOT_HTTP)               \
	FUNC(STR_INSTALL_FROM_DATA_MSG)         \
	FUNC(STR_ALREADY_INSTALLED_MSG)         \
	FUNC(STR_INSTALL_FROM_URL)              \
	FUNC(STR_CANNOT_READ_PKG_HDR_MSG)       \
	FUNC(STR_FAVORITE_URLS)                 \
	FUNC(STR_SLOT)                          \
	FUNC(STR_EDIT)                          \
	FUNC(STR_ONETIME_URL)                   \
	FUNC(STR_NOT_A_VALID_PACKAGE)           \
	FUNC(STR_WAIT_FOR_INSTALL_MSG)          \
	FUNC(STR_FAIL_INSTALL_TMP_PKG_MSG)      \
	FUNC(STR_FAIL_TO_OBTAIN_GG_DL_MSG)      \
	FUNC(STR_AUTO_DELETE_TMP_PKG)           \
	FUNC(STR_PROTOCOL_NOT_SUPPORTED)        \
	FUNC(STR_COULD_NOT_RESOLVE_HOST)        \
	FUNC(STR_EXTRACT)                       \
	FUNC(STR_EXTRACTING)                    \
	FUNC(STR_FAILED_TO_EXTRACT)             \
	FUNC(STR_EXTRACT_LOCATION)              \
	FUNC(STR_COMPRESS)                      \
	FUNC(STR_ZIP_FILE_PATH)                 \
	FUNC(STR_COMPRESSING)                   \
	FUNC(STR_ERROR_CREATE_ZIP)              \
	FUNC(STR_UNSUPPORTED_FILE_FORMAT)       \
	FUNC(STR_CUT)                           \
	FUNC(STR_COPY)                          \
	FUNC(STR_PASTE)                         \
	FUNC(STR_MOVING)                        \
	FUNC(STR_COPYING)                       \
	FUNC(STR_FAIL_MOVE_MSG)                 \
	FUNC(STR_FAIL_COPY_MSG)                 \
	FUNC(STR_CANT_MOVE_TO_SUBDIR_MSG)       \
	FUNC(STR_CANT_COPY_TO_SUBDIR_MSG)       \
	FUNC(STR_UNSUPPORTED_OPERATION_MSG)     \
	FUNC(STR_HTTP_PORT)                     \
	FUNC(STR_REINSTALL_CONFIRM_MSG)         \
	FUNC(STR_REMOTE_NOT_SUPPORT_MSG)        \
	FUNC(STR_CANNOT_CONNECT_REMOTE_MSG)     \
	FUNC(STR_DOWNLOAD_INSTALL_MSG)          \
	FUNC(STR_CHECKING_REMOTE_SERVER_MSG)    \
	FUNC(STR_ENABLE_RPI)                    \
	FUNC(STR_ENABLE_RPI_FTP_SMB_MSG)        \
	FUNC(STR_ENABLE_RPI_WEBDAV_MSG)         \
	FUNC(STR_FILES)                         \
	FUNC(STR_EDITOR)                        \
	FUNC(STR_SAVE)                          \
	FUNC(STR_MAX_EDIT_FILE_SIZE_MSG)        \
	FUNC(STR_DELETE_LINE)                   \
	FUNC(STR_INSERT_LINE)                   \
	FUNC(STR_MODIFIED)                      \
	FUNC(STR_FAIL_GET_TOKEN_MSG)            \
	FUNC(STR_GET_TOKEN_SUCCESS_MSG)         \
	FUNC(STR_PERM_DRIVE)                    \
	FUNC(STR_PERM_DRIVE_APPDATA)            \
	FUNC(STR_PERM_DRIVE_FILE)               \
	FUNC(STR_PERM_DRIVE_METADATA)           \
	FUNC(STR_PERM_DRIVE_METADATA_RO)        \
	FUNC(STR_GOOGLE_LOGIN_FAIL_MSG)         \
	FUNC(STR_GOOGLE_LOGIN_TIMEOUT_MSG)      \
	FUNC(STR_NEW_FILE)                      \
	FUNC(STR_SETTINGS)                      \
	FUNC(STR_CLIENT_ID)                     \
	FUNC(STR_CLIENT_SECRET)                 \
	FUNC(STR_GLOBAL)                        \
	FUNC(STR_GOOGLE)                        \
	FUNC(STR_COPY_LINE)                     \
	FUNC(STR_PASTE_LINE)                    \
	FUNC(STR_SHOW_HIDDEN_FILES)             \
	FUNC(STR_SET_DEFAULT_DIRECTORY)         \
	FUNC(STR_SET_DEFAULT_DIRECTORY_MSG)     \
	FUNC(STR_VIEW_IMAGE)                    \
	FUNC(STR_VIEW_PKG_INFO)                 \
	FUNC(STR_NFS_EXP_PATH_MISSING_MSG)      \
	FUNC(STR_FAIL_INIT_NFS_CONTEXT)         \
	FUNC(STR_FAIL_MOUNT_NFS_MSG)            \
	FUNC(STR_WEB_SERVER)                    \
	FUNC(STR_ENABLE)                        \
	FUNC(STR_COMPRESSED_FILE_PATH)          \
	FUNC(STR_COMPRESSED_FILE_PATH_MSG)      \
	FUNC(STR_ALLDEBRID)                     \
	FUNC(STR_API_KEY)                       \
	FUNC(STR_CANT_EXTRACT_URL_MSG)          \
	FUNC(STR_FAIL_INSTALL_FROM_URL_MSG)     \
	FUNC(STR_INVALID_URL)                   \
	FUNC(STR_ALLDEBRID_API_KEY_MISSING_MSG) \
	FUNC(STR_LANGUAGE)

#define GET_VALUE(x) x,
#define GET_STRING(x) #x,

enum
{
	FOREACH_STR(GET_VALUE)
};

#define LANG_STRINGS_NUM 159
#define LANG_ID_SIZE 64
#define LANG_STR_SIZE 384
extern char lang_identifiers[LANG_STRINGS_NUM][LANG_ID_SIZE];
extern char lang_strings[LANG_STRINGS_NUM][LANG_STR_SIZE];
extern bool needs_extended_font;

namespace Lang
{
	void SetTranslation(int32_t lang_idx);
}

#endif