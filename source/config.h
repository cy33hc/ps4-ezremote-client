#ifndef LAUNCHER_CONFIG_H
#define LAUNCHER_CONFIG_H

#include <string>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <set>

#include "clients/remote_client.h"

#define APP_ID "ezremote-client"
#define DATA_PATH "/data/" APP_ID
#define CONFIG_INI_FILE DATA_PATH "/config.ini"
#define COOKIE_FILE DATA_PATH "/cookies.txt"
#define TMP_EDITOR_FILE DATA_PATH "/tmp_editor.txt"
#define TMP_SFO_PATH DATA_PATH "/tmp_pkg.sfo"
#define TMP_ICON_PATH DATA_PATH "/tmp_icon.png"

#define CONFIG_GLOBAL "Global"

#define CONFIG_SHOW_HIDDEN_FILES "show_hidden_files"

#define CONFIG_GOOGLE "Google"
#define CONFIG_GOOGLE_CLIENT_ID "google_client_id"
#define CONFIG_GOOGLE_CLIENT_SECRET "google_client_secret"
#define CONFIG_GOOGLE_PERMISSIONS "google_client_permissions"
#define CONFIG_GOOGLE_ACCESS_TOKEN "google_access_token"
#define CONFIG_GOOGLE_REFRESH_TOKEN "google_refresh_token"
#define CONFIG_GOOGLE_TOKEN_EXPIRY "google_token_expiry"

#define GOOGLE_OAUTH_HOST "https://oauth2.googleapis.com"
#define GOOGLE_AUTH_URL "https://accounts.google.com/o/oauth2/v2/auth"
#define GOOGLE_API_URL "https://www.googleapis.com"
#define GOOGLE_DRIVE_API_PATH "/drive/v2/files"
#define GOOGLE_DRIVE_BASE_URL "https://drive.google.com"
#define GOOGLE_PERM_DRIVE "drive"
#define GOOGLE_PERM_DRIVE_APPDATA "drive.appdata"
#define GOOGLE_PERM_DRIVE_FILE "drive.file"
#define GOOGLE_PERM_DRIVE_METADATA "drive.metadata"
#define GOOGLE_PERM_DRIVE_METADATA_RO "drive.metadata.readonly"
#define GOOGLE_DEFAULT_PERMISSIONS GOOGLE_PERM_DRIVE

#define CONFIG_HTTP_SERVER "HttpServer"
#define CONFIG_HTTP_SERVER_PORT "http_server_port"
#define CONFIG_HTTP_SERVER_ENABLED "http_server_enabled"
#define CONFIG_HTTP_SERVER_COMPRESSED_FILE_PATH "compressed_files_path"
#define CONFIG_DEFAULT_COMPRESSED_FILE_PATH DATA_PATH "/compressed_files"

#define CONFIG_REMOTE_SERVER_NAME "remote_server_name"
#define CONFIG_REMOTE_SERVER_URL "remote_server_url"
#define CONFIG_REMOTE_SERVER_USER "remote_server_user"
#define CONFIG_REMOTE_SERVER_PASSWORD "remote_server_password"
#define CONFIG_REMOTE_SERVER_HTTP_PORT "remote_server_http_port"
#define CONFIG_ENABLE_RPI "remote_server_enable_rpi"
#define CONFIG_REMOTE_HTTP_SERVER_TYPE "remote_server_http_server_type"
#define CONFIG_REMOTE_DEFAULT_DIRECTORY "remote_server_default_directory"

#define CONFIG_VERSION "config_version"
#define CONFIG_VERSION_NUM 1

#define CONFIG_FAVORITE_URLS "favorite_urls"
#define MAX_FAVORITE_URLS 30
#define CONFIG_MAX_EDIT_FILE_SIZE "max_edit_file_size"

#define CONFIG_LAST_SITE "last_site"
#define CONFIG_AUTO_DELETE_TMP_PKG "auto_delete_tmp_pkg"

#define CONFIG_LOCAL_DIRECTORY "local_directory"

#define CONFIG_LANGUAGE "language"

#define HTTP_SERVER_APACHE "Apache"
#define HTTP_SERVER_MS_IIS "Microsoft IIS"
#define HTTP_SERVER_NGINX "Nginx"
#define HTTP_SERVER_NPX_SERVE "Serve"

#define MAX_EDIT_FILE_SIZE 32768

struct GoogleAccountInfo
{
    char access_token[256];
    char refresh_token[256];
    uint64_t token_expiry;
};

struct GoogleAppInfo
{
    char client_id[140];
    char client_secret[64];
    char permissions[92];
};

struct RemoteSettings
{
    char site_name[32];
    char server[256];
    char username[33];
    char password[128];
    int http_port;
    ClientType type;
    bool enable_rpi;
    uint32_t supported_actions;
    char http_server_type[24];
    GoogleAccountInfo gg_account;
    char default_directory[256];
};

struct PackageUrlInfo
{
    char url[512];
    char username[33];
    char password[25];
};

extern std::vector<std::string> sites;
extern std::vector<std::string> http_servers;
extern std::set<std::string> text_file_extensions;
extern std::set<std::string> image_file_extensions;
extern std::map<std::string, RemoteSettings> site_settings;
extern char local_directory[255];
extern char remote_directory[255];
extern char app_ver[6];
extern char last_site[32];
extern char display_site[32];
extern char language[128];
extern RemoteSettings *remote_settings;
extern RemoteClient *remoteclient;
extern PackageUrlInfo install_pkg_url;
extern char favorite_urls[MAX_FAVORITE_URLS][512];
extern bool auto_delete_tmp_pkg;
extern int max_edit_file_size;
extern unsigned char cipher_key[32];
extern unsigned char cipher_iv[16];
extern GoogleAppInfo gg_app;
extern bool show_hidden_files;

namespace CONFIG
{
    void LoadConfig();
    void SaveConfig();
    void SaveGlobalConfig();
    void SaveLocalDirecotry(const std::string &path);
    void SaveFavoriteUrl(int index, char *url);
    void SetClientType(RemoteSettings *settings);
}
#endif
