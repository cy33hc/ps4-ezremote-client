#include <orbis/UserService.h>
#include <orbis/Net.h>
#include <string>
#include <cstring>
#include <map>
#include <vector>
#include <regex>
#include <stdlib.h>
#include "server/http_server.h"
#include "config.h"
#include "fs.h"
#include "lang.h"
#include "crypt.h"
#include "base64.h"

extern "C"
{
#include "inifile.h"
}

bool swap_xo;
RemoteSettings *remote_settings;
char local_directory[255];
char remote_directory[255];
char app_ver[6];
char last_site[32];
char display_site[32];
char language[128];
std::vector<std::string> sites;
std::vector<std::string> http_servers;
std::map<std::string, RemoteSettings> site_settings;
PackageUrlInfo install_pkg_url;
char favorite_urls[MAX_FAVORITE_URLS][512];
bool auto_delete_tmp_pkg;
int max_edit_file_size;
GoogleAccountInfo gg_account;

unsigned char cipher_key[32] = {'s', '5', 'v', '8', 'y', '/', 'B', '?', 'E', '(', 'H', '+', 'M', 'b', 'Q', 'e', 'T', 'h', 'W', 'm', 'Z', 'q', '4', 't', '7', 'w', '9', 'z', '$', 'C', '&', 'F'};
unsigned char cipher_iv[16] = {'Y', 'p', '3', 's', '6', 'v', '9', 'y', '$', 'B', '&', 'E', ')', 'H', '@', 'M'};

RemoteClient *remoteclient;

namespace CONFIG
{
    int Encrypt(const std::string &text, std::string &encrypt_text)
    {
        unsigned char tmp_encrypt_text[text.length() * 2];
        int encrypt_text_len;
        memset(tmp_encrypt_text, 0, sizeof(tmp_encrypt_text));
        int ret = openssl_encrypt((unsigned char *)text.c_str(), text.length(), cipher_key, cipher_iv, tmp_encrypt_text, &encrypt_text_len);
        if (ret == 0)
            return 0;
        return Base64::Encode(std::string((const char *)tmp_encrypt_text, encrypt_text_len), encrypt_text);
    }

    int Decrypt(const std::string &text, std::string &decrypt_text)
    {
        std::string tmp_decode_text;
        int ret = Base64::Decode(text, tmp_decode_text);
        if (ret == 0)
            return 0;

        unsigned char tmp_decrypt_text[tmp_decode_text.length() * 2];
        int decrypt_text_len;
        memset(tmp_decrypt_text, 0, sizeof(tmp_decrypt_text));
        ret = openssl_decrypt((unsigned char *)tmp_decode_text.c_str(), tmp_decode_text.length(), cipher_key, cipher_iv, tmp_decrypt_text, &decrypt_text_len);
        if (ret == 0)
            return 0;

        decrypt_text.clear();
        decrypt_text.append(std::string((const char *)tmp_decrypt_text, decrypt_text_len));

        return 1;
    }

    void SetClientType(RemoteSettings *setting)
    {
        if (strncmp(setting->server, "smb://", 6) == 0)
        {
            setting->type = CLIENT_TYPE_SMB;
        }
        else if (strncmp(setting->server, "ftp://", 6) == 0)
        {
            setting->type = CLIENT_TYPE_FTP;
        }
        else if (strncmp(setting->server, "webdav://", 9) == 0 || strncmp(setting->server, "webdavs://", 10) == 0)
        {
            setting->type = CLIENT_TYPE_WEBDAV;
        }
        else if (strncmp(setting->server, "http://", 7) == 0 || strncmp(setting->server, "https://", 8) == 0)
        {
            setting->type = CLIENT_TYPE_HTTP_SERVER;
        }
        else
        {
            setting->type = CLINET_TYPE_UNKNOWN;
        }
    }

    void LoadCipherKeys()
    {
        // Get the key and iv for encryption. Inject the account_id/MAC address as part of the key and iv.
        int user_id;
        uint64_t account_id = 0;
        sceUserServiceGetForegroundUser(&user_id);
        sceUserServiceGetNpAccountId(user_id, &account_id);
        unsigned char data[sizeof(account_id)];
        memcpy(data, &account_id, sizeof(account_id));
        OrbisNetEtherAddr addr;
        memset(&addr, 0x0, sizeof(OrbisNetEtherAddr));
        sceNetGetMacAddress(&addr, 0);
        for (int i = 0; i < sizeof(data); i++)
        {
            cipher_key[i] = data[i];
            cipher_key[i + 16] = data[i];
            cipher_iv[i] = data[i];
        }
        int offset = sizeof(data);
        for (int i = 0; i < sizeof(addr.data); i++)
        {
            cipher_key[offset + i] = addr.data[i];
            cipher_key[offset + i + 16] = data[i];
            cipher_iv[offset + i] = addr.data[i];
        }
    }

    void LoadConfig()
    {
        LoadCipherKeys();

        if (!FS::FolderExists(DATA_PATH))
        {
            FS::MkDirs(DATA_PATH);
        }

        sites = {"Site 1", "Site 2", "Site 3", "Site 4", "Site 5", "Site 6", "Site 7", "Site 8", "Site 9", "Site 10",
                 "Site 11", "Site 12", "Site 13", "Site 14", "Site 15", "Site 16", "Site 17", "Site 18", "Site 19", "Site 20"};

        http_servers = {HTTP_SERVER_APACHE, HTTP_SERVER_MS_IIS, HTTP_SERVER_NGINX, HTTP_SERVER_NPX_SERVE};

        OpenIniFile(CONFIG_INI_FILE);

        int version = ReadInt(CONFIG_GLOBAL, CONFIG_VERSION, 0);
        bool conversion_needed = false;
        if (version < CONFIG_VERSION_NUM)
        {
            conversion_needed = true;
        }
        WriteInt(CONFIG_GLOBAL, CONFIG_VERSION, CONFIG_VERSION_NUM);

        // Load global config
        sprintf(language, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LANGUAGE, ""));
        WriteString(CONFIG_GLOBAL, CONFIG_LANGUAGE, language);

        sprintf(local_directory, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, "/"));
        WriteString(CONFIG_GLOBAL, CONFIG_LOCAL_DIRECTORY, local_directory);

        sprintf(remote_directory, "%s", ReadString(CONFIG_GLOBAL, CONFIG_REMOTE_DIRECTORY, "/"));
        WriteString(CONFIG_GLOBAL, CONFIG_REMOTE_DIRECTORY, remote_directory);

        auto_delete_tmp_pkg = ReadBool(CONFIG_GLOBAL, CONFIG_AUTO_DELETE_TMP_PKG, true);
        WriteBool(CONFIG_GLOBAL, CONFIG_AUTO_DELETE_TMP_PKG, auto_delete_tmp_pkg);

        max_edit_file_size = ReadInt(CONFIG_GLOBAL, CONFIG_MAX_EDIT_FILE_SIZE, MAX_EDIT_FILE_SIZE);
        WriteInt(CONFIG_GLOBAL, CONFIG_MAX_EDIT_FILE_SIZE, max_edit_file_size);

        // Load Google Account Info
        sprintf(gg_account.client_id, "%s", ReadString(CONFIG_GOOGLE, CONFIG_GOOGLE_CLIENT_ID, ""));
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_CLIENT_ID, gg_account.client_id);

        // Token Expiry
        gg_account.token_expiry = ReadLong(CONFIG_GOOGLE, CONFIG_GOOGLE_TOKEN_EXPIRY, 0);
        WriteLong(CONFIG_GOOGLE, CONFIG_GOOGLE_TOKEN_EXPIRY, gg_account.token_expiry);

        // Client Secret
        char tmp_gg_secret[512];
        sprintf(tmp_gg_secret, "%s", ReadString(CONFIG_GOOGLE, CONFIG_GOOGLE_CLIENT_SECRET, ""));
        std::string encrypted_secret;
        if (strlen(tmp_gg_secret) > 0)
        {
            std::string decrypted_secret;
            int ret = Decrypt(tmp_gg_secret, decrypted_secret);
            if (ret == 0)
                sprintf(gg_account.client_secret, "%s", tmp_gg_secret);
            else
                sprintf(gg_account.client_secret, "%s", decrypted_secret.c_str());
            Encrypt(gg_account.client_secret, encrypted_secret);
        }
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_CLIENT_SECRET, encrypted_secret.c_str());

        // Access Token
        sprintf(tmp_gg_secret, "%s", ReadString(CONFIG_GOOGLE, CONFIG_GOOGLE_ACCESS_TOKEN, ""));
        std::string encrypted_token;
        if (strlen(tmp_gg_secret) > 0)
        {
            std::string decrypted_secret;
            int ret = Decrypt(tmp_gg_secret, decrypted_secret);
            if (ret == 0)
                sprintf(gg_account.access_token, "%s", tmp_gg_secret);
            else
                sprintf(gg_account.access_token, "%s", decrypted_secret.c_str());
            Encrypt(gg_account.access_token, encrypted_token);
        }
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_ACCESS_TOKEN, encrypted_token.c_str());

        // Refresh Token
        sprintf(tmp_gg_secret, "%s", ReadString(CONFIG_GOOGLE, CONFIG_GOOGLE_REFRESH_TOKEN, ""));
        std::string encrypted_refresh_token;
        if (strlen(tmp_gg_secret) > 0)
        {
            std::string decrypted_secret;
            int ret = Decrypt(tmp_gg_secret, decrypted_secret);
            if (ret == 0)
                sprintf(gg_account.refresh_token, "%s", tmp_gg_secret);
            else
                sprintf(gg_account.refresh_token, "%s", decrypted_secret.c_str());
            Encrypt(gg_account.refresh_token, encrypted_refresh_token);
        }
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_REFRESH_TOKEN, encrypted_refresh_token.c_str());

        sprintf(gg_account.permissions, "%s", ReadString(CONFIG_GOOGLE, CONFIG_GOOGLE_PERMISSIONS, GOOGLE_DEFAULT_PERMISSIONS));
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_PERMISSIONS, gg_account.permissions);

        // Http Server Info
        http_server_port = ReadInt(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_PORT, 8080);
        WriteInt(CONFIG_HTTP_SERVER, CONFIG_HTTP_SERVER_PORT, http_server_port);

        for (int i = 0; i < sites.size(); i++)
        {
            RemoteSettings setting;
            memset(&setting, 0, sizeof(RemoteSettings));
            sprintf(setting.site_name, "%s", sites[i].c_str());

            sprintf(setting.server, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_URL, ""));
            if (conversion_needed && strlen(setting.server) > 0)
            {
                std::string tmp = std::string(setting.server);
                tmp = std::regex_replace(tmp, std::regex("http://"), "webdav://");
                tmp = std::regex_replace(tmp, std::regex("https://"), "webdavs://");
                sprintf(setting.server, "%s", tmp.c_str());
            }
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_URL, setting.server);

            sprintf(setting.username, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, ""));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_USER, setting.username);

            char tmp_password[64];
            sprintf(tmp_password, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, ""));
            std::string encrypted_password;
            if (strlen(tmp_password) > 0)
            {
                std::string decrypted_password;
                int ret = Decrypt(tmp_password, decrypted_password);
                if (ret == 0)
                    sprintf(setting.password, "%s", tmp_password);
                else
                    sprintf(setting.password, "%s", decrypted_password.c_str());
                Encrypt(setting.password, encrypted_password);
            }
            WriteString(sites[i].c_str(), CONFIG_REMOTE_SERVER_PASSWORD, encrypted_password.c_str());

            setting.http_port = ReadInt(sites[i].c_str(), CONFIG_REMOTE_SERVER_HTTP_PORT, 80);
            WriteInt(sites[i].c_str(), CONFIG_REMOTE_SERVER_HTTP_PORT, setting.http_port);

            setting.enable_rpi = ReadBool(sites[i].c_str(), CONFIG_ENABLE_RPI, false);
            WriteBool(sites[i].c_str(), CONFIG_ENABLE_RPI, setting.enable_rpi);

            sprintf(setting.http_server_type, "%s", ReadString(sites[i].c_str(), CONFIG_REMOTE_HTTP_SERVER_TYPE, HTTP_SERVER_APACHE));
            WriteString(sites[i].c_str(), CONFIG_REMOTE_HTTP_SERVER_TYPE, setting.http_server_type);

            SetClientType(&setting);
            site_settings.insert(std::make_pair(sites[i], setting));
        }

        sprintf(last_site, "%s", ReadString(CONFIG_GLOBAL, CONFIG_LAST_SITE, sites[0].c_str()));
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);

        remote_settings = &site_settings[std::string(last_site)];
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

        std::string encrypted_text;
        if (strlen(remote_settings->password) > 0)
            Encrypt(remote_settings->password, encrypted_text);
        else
            encrypted_text = std::string(remote_settings->password);
        WriteString(last_site, CONFIG_REMOTE_SERVER_URL, remote_settings->server);
        WriteString(last_site, CONFIG_REMOTE_SERVER_USER, remote_settings->username);
        WriteString(last_site, CONFIG_REMOTE_SERVER_PASSWORD, encrypted_text.c_str());
        WriteInt(last_site, CONFIG_REMOTE_SERVER_HTTP_PORT, remote_settings->http_port);
        WriteBool(last_site, CONFIG_ENABLE_RPI, remote_settings->enable_rpi);
        WriteString(last_site, CONFIG_REMOTE_HTTP_SERVER_TYPE, remote_settings->http_server_type);
        WriteString(CONFIG_GLOBAL, CONFIG_LAST_SITE, last_site);
        WriteBool(CONFIG_GLOBAL, CONFIG_AUTO_DELETE_TMP_PKG, auto_delete_tmp_pkg);
        WriteIniFile(CONFIG_INI_FILE);
        CloseIniFile();
    }

    void SaveGoolgeAccountInfo()
    {
        OpenIniFile(CONFIG_INI_FILE);

        std::string encrypted_secret;
        if (strlen(gg_account.client_secret) > 0)
            Encrypt(gg_account.client_secret, encrypted_secret);
        else
            encrypted_secret = std::string(gg_account.client_secret);
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_CLIENT_SECRET, encrypted_secret.c_str());

        std::string encrypted_token;
        if (strlen(gg_account.access_token) > 0)
            Encrypt(gg_account.access_token, encrypted_token);
        else
            encrypted_token = std::string(gg_account.access_token);
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_ACCESS_TOKEN, encrypted_token.c_str());

        std::string encrypted_refresh_token;
        if (strlen(gg_account.refresh_token) > 0)
            Encrypt(gg_account.refresh_token, encrypted_refresh_token);
        else
            encrypted_refresh_token = std::string(gg_account.refresh_token);
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_REFRESH_TOKEN, encrypted_refresh_token.c_str());

        WriteLong(CONFIG_GOOGLE, CONFIG_GOOGLE_TOKEN_EXPIRY, gg_account.token_expiry);
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_CLIENT_ID, gg_account.client_id);
        WriteString(CONFIG_GOOGLE, CONFIG_GOOGLE_PERMISSIONS, gg_account.permissions);
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
}
