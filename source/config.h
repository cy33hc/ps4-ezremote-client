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

#define CONFIG_GLOBAL "Global"

#define CONFIG_REMOTE_SERVER_NAME "remote_server_name"
#define CONFIG_REMOTE_SERVER_URL "remote_server_url"
#define CONFIG_REMOTE_SERVER_USER "remote_server_user"
#define CONFIG_REMOTE_SERVER_PASSWORD "remote_server_password"
#define CONFIG_REMOTE_SERVER_HTTP_PORT "remote_server_http_port"

#define CONFIG_FAVORITE_URLS "favorite_urls"
#define MAX_FAVORITE_URLS 30

#define CONFIG_LAST_SITE "last_site"
#define CONFIG_AUTO_DELETE_TMP_PKG "auto_delete_tmp_pkg"

#define CONFIG_LOCAL_DIRECTORY "local_directory"
#define CONFIG_REMOTE_DIRECTORY "remote_directory"

#define CONFIG_LANGUAGE "language"

struct RemoteSettings
{
    char site_name[32];
    char server[256];
    char username[33];
    char password[25];
    int http_port;
    bool is_smb;
    bool is_ftp;
};

struct PackageUrlInfo
{
    char url[512];
    char username[33];
    char password[25];
};

extern std::vector<std::string> sites;
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

namespace CONFIG
{
    void LoadConfig();
    void SaveConfig();
    void SaveFavoriteUrl(int index, char *url);
    void RemoveFromMultiValues(std::vector<std::string> &multi_values, std::string value);
    void ParseMultiValueString(const char *prefix_list, std::vector<std::string> &prefixes, bool toLower);
    std::string GetMultiValueString(std::vector<std::string> &multi_values);
}
#endif
