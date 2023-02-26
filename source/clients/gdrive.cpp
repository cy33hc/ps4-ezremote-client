#include <orbis/UserService.h>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <json-c/json.h>
#include "config.h"
#include "common.h"
#include "server/http_server.h"
#include "clients/remote_client.h"
#include "clients/gdrive.h"
#include "lang.h"
#include "util.h"
#include "windows.h"
#include "system.h"
#include "dbglogger.h"

using namespace httplib;

std::string GetRedirectUrl()
{
    return std::string("https://localhost:" + std::to_string(http_server_port) + "/google_auth");
}

std::string GetScopes()
{
    std::vector<std::string> permissions = Util::Split(gg_account.permissions, ",");
    std::string scopes;
    for (int i = 0; i < permissions.size(); i++)
    {
        scopes.append("https://www.googleapis.com/auth/");
        scopes.append(permissions[i]);
        if (i < permissions.size() - 1)
        {
            scopes.append(" ");
        }
    }
    return scopes;
}

void RefreshAccessToken()
{
    SSLClient client(GOOGLE_OAUTH_HOST);
    client.enable_server_certificate_verification(false);
    std::string url = std::string("/token?client_id=") + BaseClient::EncodeUrl(gg_account.client_id) + "&client_secret=" +
                      BaseClient::EncodeUrl(gg_account.client_secret) + "&grant_type=refresh_token&refresh_token=" + 
                      BaseClient::EncodeUrl(gg_account.refresh_token);
    Result result = client.Post(url);

    if (result.error() == Error::Success && result.value().status == 200)
    {
        json_object *jobj = json_tokener_parse(result.value().body.c_str());
        enum json_type type;
        json_object_object_foreach(jobj, key, val)
        {
            if (strcmp(key, "access_token") == 0)
                snprintf(gg_account.access_token, 255, "%s", json_object_get_string(val));
            else if (strcmp(key, "expires_in") == 0)
            {
                OrbisTick tick;
                sceRtcGetCurrentTick(&tick);
                gg_account.token_expiry = tick.mytick + (json_object_get_int(val) * 1000000);
            }
        }
        dbglogger_log("token refreshed");
        CONFIG::SaveGoolgeAccountInfo();
    }
}

int login_state;

std::string GetValue(const std::map<std::string, std::string> &options, const std::string &name)
{
    auto it = options.find(name);
    if (it == options.end())
    {
        return std::string{""};
    }
    else
    {
        return it->second;
    }
}

GDriveClient::GDriveClient()
{
    path_id_map.insert(std::make_pair("/", "root"));
}

int GDriveClient::Connect(const std::string &url, const std::string &user, const std::string &pass)
{
    OrbisTick tick;
    sceRtcGetCurrentTick(&tick);
    if (gg_account.token_expiry < (tick.mytick - 300000000))
    {
        SceShellUIUtilLaunchByUriParam param;
        param.size = sizeof(SceShellUIUtilLaunchByUriParam);
        sceUserServiceGetForegroundUser((int *)&param.userId);

        std::string auth_url = std::string(GOOGLE_AUTH_URL "?client_id=") + gg_account.client_id + "&redirect_uri=" + GetRedirectUrl() +
                               "&response_type=code&access_type=offline&scope=" + GetScopes() + "&include_granted_scopes=true";
        auth_url = EncodeUrl(auth_url);
        std::string launch_uri = std::string("pswebbrowser:search?url=") + auth_url;
        int ret = sceShellUIUtilLaunchByUri(launch_uri.c_str(), &param);

        login_state = 0;
        OrbisTick tick;
        sceRtcGetCurrentTick(&tick);
        while (login_state == 0)
        {
            OrbisTick cur_tick;
            sceRtcGetCurrentTick(&cur_tick);
            if (cur_tick.mytick - tick.mytick > 120000000)
            {
                login_state = -2;
                break;
            }
            sceKernelUsleep(100000);
        }

        if (login_state == -1)
        {
            sprintf(response, "%s", lang_strings[STR_GOOGLE_LOGIN_FAIL_MSG]);
            return 0;
        }
        else if (login_state == -2)
        {
            sprintf(response, "%s", lang_strings[STR_GOOGLE_LOGIN_TIMEOUT_MSG]);
            return 0;
        }
    }
    StartRefreshToken();

    client = new Client(GOOGLE_API_URL);
    client->set_bearer_token_auth(gg_account.access_token);
    client->set_keep_alive(true);
    client->set_follow_location(true);
    client->set_connection_timeout(30);
    client->set_read_timeout(30);
    client->enable_server_certificate_verification(false);
    this->connected = true;

    return 1;
}

int GDriveClient::Rename(const std::string &src, const std::string &dst)
{
    std::string id = GetValue(path_id_map, src);
    std::string url = std::string("/drive/v3/files/") + BaseClient::EncodeUrl(id);
    std::string filename = dst.substr(dst.find_last_of("/")+1);
    std::string body = "{\"name\" : \"" + filename +"\"}";
    if (auto res = client->Patch(url, body.c_str(), body.length(), "application/json; charset=UTF-8"))
    {
        sprintf(response, "%d", res->status);
        path_id_map.erase(src);
        path_id_map.insert(std::make_pair(dst, id));
        dbglogger_log("res=%s", res->body.c_str());
    }
    else
    {
        sprintf(response, "%s", to_string(res.error()).c_str());
        return 0;
    }
    return 1;
}

std::vector<DirEntry> GDriveClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    memset(&entry, 0, sizeof(DirEntry));
    if (path[path.length() - 1] == '/' && path.length() > 1)
    {
        strlcpy(entry.directory, path.c_str(), path.length() - 1);
    }
    else
    {
        sprintf(entry.directory, "%s", path.c_str());
    }
    sprintf(entry.name, "..");
    sprintf(entry.path, "%s", entry.directory);
    sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
    entry.file_size = 0;
    entry.isDir = true;
    entry.selectable = false;
    out.push_back(entry);

    std::string id = GetValue(path_id_map, path);
    std::string url = std::string("/drive/v3/files?q=") + BaseClient::EncodeUrl("\"" + id + "\" in parents") +
        "&pageSize=1000&fields=" + BaseClient::EncodeUrl("files(id,mimeType,name,modifiedTime,size)");
    if (auto res = client->Get(url))
    {
        json_object *jobj = json_tokener_parse(res->body.c_str());
        json_object *files = json_object_object_get(jobj, "files");
        if (json_object_get_type(files) == json_type_array)
        {
            struct array_list *afiles = json_object_get_array(files);
            for (size_t idx = 0; idx < afiles->length; ++idx)
            {
                json_object *file = (json_object*)array_list_get_idx(afiles, idx);
                DirEntry entry;
                memset(&entry, 0, sizeof(DirEntry));

                sprintf(entry.directory, "%s", path.c_str());
                entry.selectable = true;
                entry.file_size = 0;

                const char *id = json_object_get_string(json_object_object_get(file, "id"));
                const char *name = json_object_get_string(json_object_object_get(file, "name"));
                const char *mime_type = json_object_get_string(json_object_object_get(file, "mimeType"));
                const char *modified_time = json_object_get_string(json_object_object_get(file, "modifiedTime"));

                snprintf(entry.name, 255, "%s", name);
                if (path.length() > 0 && path[path.length() - 1] == '/')
                {
                    snprintf(entry.path, 767, "%s%s", path.c_str(), entry.name);
                }
                else
                {
                    sprintf(entry.path, "%s/%s", path.c_str(), entry.name);
                }
                path_id_map.insert(std::make_pair(entry.path, id));

                if (strncmp(mime_type, "application/vnd.google-apps.folder", 35) != 0)
                {
                    entry.file_size = json_object_get_uint64(json_object_object_get(file, "size"));
                    entry.isDir = false;
                    DirEntry::SetDisplaySize(&entry);
                }
                else
                {
                    entry.isDir = true;
                    sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
                }

                std::vector<std::string> date_time_arr = Util::Split(modified_time, "T");
                std::vector<std::string> adate = Util::Split(date_time_arr[0], "-");
                std::vector<std::string> atime = Util::Split(Util::Split(date_time_arr[1], ".")[0], ":");
                OrbisDateTime utc, local;
                utc.year = std::atoi(adate[0].c_str());
                utc.month = std::atoi(adate[1].c_str());
                utc.day = std::atoi(adate[2].c_str());
                utc.hour = std::atoi(atime[0].c_str());
                utc.minute = std::atoi(atime[1].c_str());
                utc.second = std::atoi(atime[2].c_str());

                convertUtcToLocalTime(&utc, &local);

                entry.modified.year = local.year;
                entry.modified.month = local.month;
                entry.modified.day = local.day;
                entry.modified.hours = local.hour;
                entry.modified.minutes = local.minute;
                entry.modified.seconds = local.second;

                out.push_back(entry);
            }
        }
    }
    else
    {
        sprintf(response, "%s", to_string(res.error()).c_str());
    }
    return out;
}

ClientType GDriveClient::clientType()
{
    return CLIENT_TYPE_GOOGLE;
}

uint32_t GDriveClient::SupportedActions()
{
    return REMOTE_ACTION_ALL;
}

void *GDriveClient::RefreshTokenThread(void *argp)
{
    while (refresh_token_running)
    {
        OrbisTick tick;
        sceRtcGetCurrentTick(&tick);
        if (tick.mytick >= (gg_account.token_expiry - 300000000)) // refresh token 5mins before expiry
        {
            RefreshAccessToken();
        }
        sceKernelUsleep(5000000); // sleep for 5s
    }
    return NULL;
}

void GDriveClient::StartRefreshToken()
{
    if (refresh_token_running)
        return;

    refresh_token_running = true;
    int ret = pthread_create(&refresh_token_thid, NULL, RefreshTokenThread, NULL);
    if (ret != 0)
    {
        dbglogger_log("Failed to start refresh token thread");
    }
}

void GDriveClient::StopRefreshToken()
{
    refresh_token_running = false;
}