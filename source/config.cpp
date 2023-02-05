#include <string>
#include <cstring>
#include <map>
#include <vector>
#include <stdlib.h>
#include "config.h"
#include "fs.h"
#include "lang.h"

extern "C"
{
#include "inifile.h"
}

bool swap_xo;
WebDavSettings *webdav_settings;
char local_directory[255];
char remote_directory[255];
char app_ver[6];
char last_site[32];
char display_site[32];
char language[128];
std::vector<std::string> sites;
std::map<std::string, WebDavSettings> site_settings;
char install_pkg_url[512];
char favorite_urls[MAX_FAVORITE_URLS][512];
bool auto_delete_tmp_pkg;
RemoteClient *webdavclient;

namespace CONFIG
{

    void LoadConfig()
    {
        if (!FS::FolderExists(DATA_PATH))
        {
            FS::MkDirs(DATA_PATH);
        }

        sites = {"Site 1", "Site 2", "Site 3", "Site 4", "Site 5", "Site 6", "Site 7", "Site 8", "Site 9", "Site 10",
                 "Site 11", "Site 12", "Site 13", "Site 14", "Site 15", "Site 16", "Site 17", "Site 18", "Site 19", "Site 20"};

        OpenIniFile(CONFIG_INI_FILE);

        // Load global config
        sprintf(language, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LANGUAGE, ""));
        WriteString(CONFIG_GLOBAL, CONFIG_LANGUAGE, language);

        sprintf(local_directory, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, "/"));
        WriteString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, local_directory);

        sprintf(remote_directory, "%s", ReadString(CONFIG_GLOBAL, CONFIG_REMOTE_DIRECTORY, "/"));
        WriteString(CONFIG_GLOBAL, CONFIG_REMOTE_DIRECTORY, remote_directory);

        auto_delete_tmp_pkg = ReadBool(CONFIG_GLOBAL, CONFIG_AUTO_DELETE_TMP_PKG, true);
        WriteBool(CONFIG_GLOBAL, CONFIG_AUTO_DELETE_TMP_PKG, auto_delete_tmp_pkg);

        for (int i = 0; i < sites.size(); i++)
        {
            WebDavSettings setting;
            sprintf(setting.site_name, "%s", sites[i].c_str());

            sprintf(setting.server, "%s", ReadString(sites[i].c_str(), CONFIG_WEBDAV_SERVER_IP, ""));
            WriteString(sites[i].c_str(), CONFIG_WEBDAV_SERVER_IP, setting.server);

            sprintf(setting.username, "%s", ReadString(sites[i].c_str(), CONFIG_WEBDAV_SERVER_USER, ""));
            WriteString(sites[i].c_str(), CONFIG_WEBDAV_SERVER_USER, setting.username);

            sprintf(setting.password, "%s", ReadString(sites[i].c_str(), CONFIG_WEBDAV_SERVER_PASSWORD, ""));
            WriteString(sites[i].c_str(), CONFIG_WEBDAV_SERVER_PASSWORD, setting.password);

            site_settings.insert(std::make_pair(sites[i], setting));
        }

        sprintf(last_site, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LAST_SITE, sites[0].c_str()));
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);

        webdav_settings = &site_settings[std::string(last_site)];

        for (int i = 0; i < MAX_FAVORITE_URLS; i++)
        {
            const char *index = std::to_string(i).c_str();
            sprintf(favorite_urls[i], "%s", ReadString(CONFIG_FAVORITE_URLS, index, ""));
            WriteString(CONFIG_FAVORITE_URLS, index, favorite_urls[i]);
        }

        WriteIniFile(CONFIG_INI_FILE);
        CloseIniFile();
    }

    void SaveConfig()
    {
        OpenIniFile(CONFIG_INI_FILE);

        WriteString(last_site, CONFIG_WEBDAV_SERVER_IP, webdav_settings->server);
        WriteString(last_site, CONFIG_WEBDAV_SERVER_USER, webdav_settings->username);
        WriteString(last_site, CONFIG_WEBDAV_SERVER_PASSWORD, webdav_settings->password);
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);
        WriteBool(CONFIG_GLOBAL, CONFIG_AUTO_DELETE_TMP_PKG, auto_delete_tmp_pkg);
        WriteIniFile(CONFIG_INI_FILE);
        CloseIniFile();
    }

    void SaveFavoriteUrl(int index, char *url)
    {
        OpenIniFile(CONFIG_INI_FILE);
        const char *idx = std::to_string(index).c_str();
        WriteString(CONFIG_FAVORITE_URLS, idx, url);
        WriteIniFile(CONFIG_INI_FILE);
        CloseIniFile();
    }

    void ParseMultiValueString(const char *prefix_list, std::vector<std::string> &prefixes, bool toLower)
    {
        std::string prefix = "";
        int length = strlen(prefix_list);
        for (int i = 0; i < length; i++)
        {
            char c = prefix_list[i];
            if (c != ' ' && c != '\t' && c != ',')
            {
                if (toLower)
                {
                    prefix += std::tolower(c);
                }
                else
                {
                    prefix += c;
                }
            }

            if (c == ',' || i == length - 1)
            {
                prefixes.push_back(prefix);
                prefix = "";
            }
        }
    }

    std::string GetMultiValueString(std::vector<std::string> &multi_values)
    {
        std::string vts = std::string("");
        if (multi_values.size() > 0)
        {
            for (int i = 0; i < multi_values.size() - 1; i++)
            {
                vts.append(multi_values[i]).append(",");
            }
            vts.append(multi_values[multi_values.size() - 1]);
        }
        return vts;
    }

    void RemoveFromMultiValues(std::vector<std::string> &multi_values, std::string value)
    {
        auto itr = std::find(multi_values.begin(), multi_values.end(), value);
        if (itr != multi_values.end())
            multi_values.erase(itr);
    }
}
