#ifndef LAUNCHER_CONFIG_H
#define LAUNCHER_CONFIG_H

#include <string>
#include <vector>
#include <algorithm>
#include <map>

#include "remote_client.h"

#define APP_ID "ezremote-client"
#define DATA_PATH "/data/" APP_ID
#define CONFIG_INI_FILE DATA_PATH "/config.ini"
#define COOKIE_FILE DATA_PATH "/cookies.txt"
#define TMP_EDITOR_FILE DATA_PATH "/tmp_editor.txt"

#define CONFIG_GLOBAL "Global"

#define CONFIG_REMOTE_SERVER_NAME "remote_server_name"
#define CONFIG_REMOTE_SERVER_URL "remote_server_url"
#define CONFIG_REMOTE_SERVER_USER "remote_server_user"
#define CONFIG_REMOTE_SERVER_PASSWORD "remote_server_password"
#define CONFIG_REMOTE_SERVER_HTTP_PORT "remote_server_http_port"
#define CONFIG_ENABLE_RPI "remote_server_enable_rpi"
#define CONFIG_REMOTE_HTTP_SERVER_TYPE "remote_server_http_server_type"

#define CONFIG_VERSION "config_version"
#define CONFIG_VERSION_NUM 1

#define CONFIG_FAVORITE_URLS "favorite_urls"
#define MAX_FAVORITE_URLS 30
#define CONFIG_MAX_EDIT_FILE_SIZE "max_edit_file_size"

#define CONFIG_LAST_SITE "last_site"
#define CONFIG_AUTO_DELETE_TMP_PKG "auto_delete_tmp_pkg"

#define CONFIG_LOCAL_DIRECTORY "local_directory"
#define CONFIG_REMOTE_DIRECTORY "remote_directory"

#define CONFIG_LANGUAGE "language"

#define HTTP_SERVER_APACHE "Apache"
#define HTTP_SERVER_MS_IIS "Microsoft IIS"
#define HTTP_SERVER_NGINX "Nginx"
#define HTTP_SERVER_NPX_SERVE "Serve"

#define MAX_EDIT_FILE_SIZE 32768

struct RemoteSettings
{
    char site_name[32];
    char server[256];
    char username[33];
    char password[64];
    int http_port;
    ClientType type;
    bool enable_rpi;
    uint32_t supported_actions;
    char http_server_type[24];
};

struct PackageUrlInfo
{
    char url[512];
    char username[33];
    char password[25];
};

extern std::vector<std::string> sites;
extern std::vector<std::string> http_servers;
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

namespace CONFIG
{
    void LoadConfig();
    void SaveConfig();
    void SaveFavoriteUrl(int index, char *url);
    void SetClientType(RemoteSettings *settings);
    void RemoveFromMultiValues(std::vector<std::string> &multi_values, std::string value);
    void ParseMultiValueString(const char *prefix_list, std::vector<std::string> &prefixes, bool toLower);
    std::string GetMultiValueString(std::vector<std::string> &multi_values);
}
#endif
