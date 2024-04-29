#include <orbis/UserService.h>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <json-c/json.h>
#include "config.h"
#include "common.h"
#include "server/http_server.h"
#include "clients/remote_client.h"
#include "clients/gdrive.h"
#include "fs.h"
#include "lang.h"
#include "util.h"
#include "windows.h"
#include "system.h"

#define GOOGLE_BUF_SIZE 262144

static std::string shared_with_me("Shared with me");
static std::string my_drive("My Drive");

using namespace httplib;

std::string GetRedirectUrl()
{
    return std::string("http://localhost:" + std::to_string(http_server_port) + "/google_auth");
}

std::string GetScopes()
{
    std::vector<std::string> permissions = Util::Split(gg_app.permissions, ",");
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

int RefreshAccessToken()
{
    Client client(GOOGLE_OAUTH_HOST);
    client.enable_server_certificate_verification(false);
    client.set_follow_location(true);
    std::string url = std::string("/token");
    std::string post_data = std::string("grant_type=refresh_token") +
                            "&client_id=" + gg_app.client_id +
                            "&client_secret=" + gg_app.client_secret +
                            "&refresh_token=" + remote_settings->gg_account.refresh_token;

    if (auto res = client.Post(url, post_data.c_str(), post_data.length(), "application/x-www-form-urlencoded"))
    {
        if (HTTP_SUCCESS(res->status))
        {
            json_object *jobj = json_tokener_parse(res->body.c_str());
            enum json_type type;
            json_object_object_foreach(jobj, key, val)
            {
                if (strcmp(key, "access_token") == 0)
                    snprintf(remote_settings->gg_account.access_token, 255, "%s", json_object_get_string(val));
                else if (strcmp(key, "expires_in") == 0)
                {
                    OrbisTick tick;
                    sceRtcGetCurrentTick(&tick);
                    remote_settings->gg_account.token_expiry = tick.mytick + (json_object_get_uint64(val) * 1000000);
                }
            }
            if (remoteclient != nullptr && remoteclient->clientType() == CLIENT_TYPE_GOOGLE)
            {
                GDriveClient *client = (GDriveClient *)remoteclient;
                client->SetAccessToken(remote_settings->gg_account.access_token);
            }
            CONFIG::SaveConfig();
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
    return 1;
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

int GDriveClient::RequestAuthorization()
{
    SceShellUIUtilLaunchByUriParam param;
    param.size = sizeof(SceShellUIUtilLaunchByUriParam);
    sceUserServiceGetForegroundUser((int *)&param.userId);

    std::string auth_url = std::string(GOOGLE_AUTH_URL "?client_id=") + gg_app.client_id + "&redirect_uri=" + GetRedirectUrl() +
                           "&response_type=code&access_type=offline&scope=" + GetScopes() + "&include_granted_scopes=true";
    auth_url = Escape(auth_url);
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

    return 1;
}

std::string GDriveClient::GetDriveId(const std::string path)
{
    size_t slash_pos = path.find("/", 1);
    std::string root_path = path.substr(0, slash_pos);
    std::string id = GetValue(shared_drive_map, root_path);
    return id;
}

GDriveClient::GDriveClient()
{
    client = nullptr;
    path_id_map.insert(std::make_pair("/" + my_drive, "root"));
    path_id_map.insert(std::make_pair("/" + shared_with_me, shared_with_me));
}

int GDriveClient::Connect(const std::string &url, const std::string &user, const std::string &pass)
{
    if (strlen(remote_settings->gg_account.refresh_token) > 0)
    {
        int ret = RefreshAccessToken();
        if (ret == 0)
        {
            RequestAuthorization();
        }
    }
    else
    {
        RequestAuthorization();
    }
    StartRefreshToken();

    client = new Client(GOOGLE_API_URL);
    client->set_bearer_token_auth(remote_settings->gg_account.access_token);
    client->set_keep_alive(true);
    client->set_follow_location(true);
    client->set_connection_timeout(30);
    client->set_read_timeout(30);
    client->enable_server_certificate_verification(false);
    this->connected = true;

    std::string drives_url = std::string("/drive/v3/drives?pageSize=100");
    if (auto res = client->Get(drives_url))
    {
        if (HTTP_SUCCESS(res->status))
        {
            json_object *jobj = json_tokener_parse(res->body.c_str());
            json_object *drives = json_object_object_get(jobj, "drives");
            if (json_object_get_type(drives) == json_type_array)
            {
                struct array_list *adrives = json_object_get_array(drives);
                for (size_t idx = 0; idx < adrives->length; ++idx)
                {
                    json_object *drive = (json_object *)array_list_get_idx(adrives, idx);
                    const char *id = json_object_get_string(json_object_object_get(drive, "id"));
                    const char *name = json_object_get_string(json_object_object_get(drive, "name"));
                    shared_drive_map.insert(std::make_pair("/" + std::string(name), id));
                    path_id_map.insert(std::make_pair("/" + std::string(name), id));
                }
            }
        }
    }

    return 1;
}

int GDriveClient::Rename(const std::string &src, const std::string &dst)
{
    if (src.find("/" + shared_with_me) != std::string::npos || dst.find("/" + shared_with_me) != std::string::npos)
        return 0;

    std::string src_id = GetValue(path_id_map, src);
    std::string dst_id = GetValue(path_id_map, dst);
    std::string src_drive_id = GetDriveId(src);
    std::string dst_drive_id = GetDriveId(dst);

    if (src_id.compare("root") == 0 || dst_id.compare("root") == 0 || src_id.compare(src_drive_id) == 0 || dst_id.compare(dst_drive_id) == 0)
        return 0;

    std::string url = std::string("/drive/v3/files/") + BaseClient::Escape(src_id);
    if (!src_drive_id.empty())
        url += "?supportsAllDrives=true";
    std::string filename = dst.substr(dst.find_last_of("/") + 1);
    std::string body = "{\"name\" : \"" + filename + "\"}";
    if (auto res = client->Patch(url, body.c_str(), body.length(), "application/json; charset=UTF-8"))
    {
        sprintf(response, "%d", res->status);
        if (HTTP_SUCCESS(res->status))
        {
            path_id_map.erase(src);
            path_id_map.insert(std::make_pair(dst, src_id));
        }
        else
            return 0;
    }
    else
    {
        sprintf(response, "%s", to_string(res.error()).c_str());
        return 0;
    }
    return 1;
}

bool GDriveClient::FileExists(const std::string &path)
{
    std::string id = GetValue(path_id_map, path);
    if (id.empty()) // then find it parent folder to see if it exists
    {
        size_t name_separator = path.find_last_of("/");
        std::string parent = path.substr(0, name_separator);
        if (parent.empty())
            parent = "/";

        if (FileExists(parent))
        {
            ListDir(parent);
            id = GetValue(path_id_map, path);
            if (!id.empty())
                return true;
        }
    }
    else
        return true;
    return false;
}

int GDriveClient::Head(const std::string &path, void *buffer, uint64_t len)
{
    size_t bytes_read = 0;
    std::vector<char> body;
    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);

    std::string url = std::string("/drive/v3/files/") + BaseClient::Escape(id) + "?alt=media";
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";
    Headers headers;
    headers.insert(std::make_pair("Range", "bytes=" + std::to_string(0) + "-" + std::to_string(len - 1)));
    if (auto res = client->Get(url, headers,
                               [&](const char *data, size_t data_length)
                               {
                                   body.insert(body.end(), data, data + data_length);
                                   bytes_read += data_length;
                                   if (bytes_read > len)
                                       return false;
                                   return true;
                               }))
    {
        if (body.size() < len)
            return 0;
        memcpy(buffer, body.data(), len);
        return 1;
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
    }
    return 0;
}

int GDriveClient::Get(const std::string &outputfile, const std::string &path, uint64_t offset)
{
    std::ofstream file_stream(outputfile, std::ios::binary);
    bytes_transfered = 0;

    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);
    std::string url = std::string("/drive/v3/files/") + BaseClient::Escape(id) + "?alt=media";
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";
    if (auto res = client->Get(url,
                               [&](const char *data, size_t data_length)
                               {
                                   file_stream.write(data, data_length);
                                   bytes_transfered += data_length;
                                   return true;
                               }))
    {
        file_stream.close();
        return 1;
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
    }
    return 0;
}

int GDriveClient::GetRange(const std::string &path, DataSink &sink, uint64_t size, uint64_t offset)
{
    size_t bytes_read = 0;
    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);

    std::string url = std::string("/drive/v3/files/") + BaseClient::Escape(id) + "?alt=media";
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";
    Headers headers;
    headers.insert(std::make_pair("Range", "bytes=" + std::to_string(offset) + "-" + std::to_string(offset + size - 1)));
    if (auto res = client->Get(url, headers,
                               [&](const char *data, size_t data_length)
                               {
                                   bytes_read += data_length;
                                   bool ok = sink.write(data, data_length);
                                   return ok;
                               }))
    {
        return bytes_read == size;
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
    }
    return 0;

}

int GDriveClient::GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset)
{
    size_t bytes_read = 0;
    std::vector<char> body;
    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);

    std::string url = std::string("/drive/v3/files/") + BaseClient::Escape(id) + "?alt=media";
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";
    Headers headers;
    headers.insert(std::make_pair("Range", "bytes=" + std::to_string(offset) + "-" + std::to_string(offset + size - 1)));
    if (auto res = client->Get(url, headers,
                               [&](const char *data, size_t data_length)
                               {
                                   body.insert(body.end(), data, data + data_length);
                                   bytes_read += data_length;
                                   if (bytes_read > size)
                                       return false;
                                   return true;
                               }))
    {
        if (body.size() != size)
            return 0;
        memcpy(buffer, body.data(), size);
        return 1;
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
    }
    return 0;

}

int GDriveClient::Update(const std::string &inputfile, const std::string &path)
{
    bytes_to_download = FS::GetSize(inputfile);
    bytes_transfered = 0;

    std::ifstream file_stream(inputfile, std::ios::binary);
    bytes_transfered = 0;

    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);

    std::string url = "/upload/drive/v3/files/" + BaseClient::Escape(id) + "?uploadType=resumable";
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";
    Headers headers;
    headers.insert(std::make_pair("X-Upload-Content-Type", "application/octet-stream"));
    headers.insert(std::make_pair("X-Upload-Content-Length", std::to_string(bytes_to_download)));
    char *buf = new char[GOOGLE_BUF_SIZE];
    if (auto res = client->Patch(url))
    {
        if (HTTP_SUCCESS(res->status))
        {
            std::string upload_uri = res->get_header_value("location");
            upload_uri = std::regex_replace(upload_uri, std::regex(GOOGLE_API_URL), "");
            Headers headers;
            headers.insert(std::make_pair("Content-Length", std::to_string(bytes_to_download)));
            std::string range_value = "bytes 0-" + std::to_string(bytes_to_download - 1) + "/" + std::to_string(bytes_to_download);
            headers.insert(std::make_pair("Content-Range", range_value));

            if (auto res = client->Put(
                    upload_uri, bytes_to_download,
                    [&file_stream, &buf](size_t offset, size_t length, DataSink &sink)
                    {
                        uint32_t count = 0;
                        uint32_t bytes_to_transfer = MIN(GOOGLE_BUF_SIZE, length - count);
                        do
                        {
                            file_stream.read(buf, bytes_to_transfer);
                            sink.write(buf, bytes_to_transfer);
                            count += bytes_to_transfer;
                            bytes_transfered += bytes_to_transfer;
                            bytes_to_transfer = MIN(GOOGLE_BUF_SIZE, length - count);
                        } while (count < length);
                        return true;
                    },
                    "application/octet-stream"))
            {
                // success
            }
            else
            {
                delete[] buf;
                file_stream.close();
                return 0;
            }
        }
        else
        {
            delete[] buf;
            file_stream.close();
            return 0;
        }
    }
    delete[] buf;
    file_stream.close();
    return 1;
}

int GDriveClient::Put(const std::string &inputfile, const std::string &path, uint64_t offset)
{
    if (path.find("/" + shared_with_me) != std::string::npos || path.compare("/") == 0)
        return 0;

    if (FileExists(path))
        return Update(inputfile, path);

    bytes_to_download = FS::GetSize(inputfile);
    bytes_transfered = 0;

    std::ifstream file_stream(inputfile, std::ios::binary);
    bytes_transfered = 0;

    size_t path_pos = path.find_last_of("/");
    std::string parent_dir;
    if (path_pos == 0)
        parent_dir = "/";
    else
        parent_dir = path.substr(0, path_pos);

    std::string filename = path.substr(path_pos + 1);
    std::string parent_id = GetValue(path_id_map, parent_dir);
    std::string drive_id = GetDriveId(path);

    std::string url = "/upload/drive/v3/files?uploadType=resumable";
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";

    std::string post_data = std::string("{\"name\": \"") + filename + "\"," +
                                        (drive_id.empty() ? "" : "\"driveId\" : \"" + drive_id + "\",") +
                                        "\"parents\": [\"" + parent_id + "\"]}";
    Headers headers;
    headers.insert(std::make_pair("X-Upload-Content-Type", "application/octet-stream"));
    headers.insert(std::make_pair("X-Upload-Content-Length", std::to_string(bytes_to_download)));
    char *buf = new char[GOOGLE_BUF_SIZE];
    if (auto res = client->Post(url, headers, post_data.c_str(), post_data.length(), "application/json"))
    {
        if (HTTP_SUCCESS(res->status))
        {
            std::string upload_uri = res->get_header_value("location");
            upload_uri = std::regex_replace(upload_uri, std::regex(GOOGLE_API_URL), "");
            Headers headers;
            headers.insert(std::make_pair("Content-Length", std::to_string(bytes_to_download)));
            std::string range_value = "bytes 0-" + std::to_string(bytes_to_download - 1) + "/" + std::to_string(bytes_to_download);
            headers.insert(std::make_pair("Content-Range", range_value));

            if (auto res = client->Put(
                    upload_uri, bytes_to_download,
                    [&file_stream, &buf](size_t offset, size_t length, DataSink &sink)
                    {
                        uint32_t count = 0;
                        uint32_t bytes_to_transfer = MIN(GOOGLE_BUF_SIZE, length - count);
                        do
                        {
                            file_stream.read(buf, bytes_to_transfer);
                            sink.write(buf, bytes_to_transfer);
                            count += bytes_to_transfer;
                            bytes_transfered += bytes_to_transfer;
                            bytes_to_transfer = MIN(GOOGLE_BUF_SIZE, length - count);
                        } while (count < length);
                        return true;
                    },
                    "application/octet-stream"))
            {
                // success
            }
            else
            {
                delete[] buf;
                file_stream.close();
                return 0;
            }
        }
        else
        {
            delete[] buf;
            file_stream.close();
            return 0;
        }
    }
    delete[] buf;
    file_stream.close();
    return 1;
}

int GDriveClient::Size(const std::string &path, int64_t *size)
{
    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);
    std::string url = std::string("/drive/v3/files/") + BaseClient::Escape(id) + "?fields=size";
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";
    if (auto res = client->Get(url))
    {
        sprintf(response, "%d", res->status);
        if (HTTP_SUCCESS(res->status))
        {
            json_object *jobj = json_tokener_parse(res->body.c_str());
            *size = json_object_get_uint64(json_object_object_get(jobj, "size"));
        }
        else
            return 0;
    }
    else
    {
        sprintf(response, "%s", to_string(res.error()).c_str());
        return 0;
    }
    return 1;
}

int GDriveClient::Mkdir(const std::string &path)
{
    if (path.find("/" + shared_with_me) != std::string::npos || path.find_last_of("/") == 0)
        return 0;

    // if path already exists return;
    if (FileExists(path))
        return 1;

    size_t path_pos = path.find_last_of("/");
    std::string parent_dir;
    if (path_pos == 0)
        parent_dir = "/";
    else
        parent_dir = path.substr(0, path_pos);

    std::string folder_name = path.substr(path_pos + 1);
    std::string parent_id = GetValue(path_id_map, parent_dir);
    std::string drive_id = GetDriveId(path);

    // if parent dir does not exists, create it first
    if (parent_id.length() == 0 || parent_id.empty())
    {
        Mkdir(parent_dir);
        parent_id = GetValue(path_id_map, parent_dir);
    }

    std::string url = std::string("/drive/v3/files?fields=id");
    if (!drive_id.empty())
        url += "&supportsAllDrives=true";
    std::string folder_metadata = "{\"name\" : \"" + folder_name + "\"," +
                                  "\"parents\" : [\"" + parent_id + "\"]," +
                                  (drive_id.empty() ? "" : "\"driveId\" : \"" + drive_id + "\",") + 
                                  "\"mimeType\" : \"application/vnd.google-apps.folder\"}";

    if (auto res = client->Post(url, folder_metadata.c_str(), folder_metadata.length(), "application/json; charset=UTF-8"))
    {
        sprintf(response, "%d", res->status);
        if (HTTP_SUCCESS(res->status))
        {
            json_object *jobj = json_tokener_parse(res->body.c_str());
            const char *id = json_object_get_string(json_object_object_get(jobj, "id"));
            path_id_map.insert(std::make_pair(path, id));
        }
        else
            return 0;
    }
    else
    {
        sprintf(response, "%s", to_string(res.error()).c_str());
        return 0;
    }
    return 1;
}

/*
 * Rmdir in google drive deletes all files/folders in subdirectories also.
 * Delete file/folder is the same api
 */
int GDriveClient::Rmdir(const std::string &path, bool recursive)
{
    if (path.find("/" + shared_with_me) != std::string::npos)
        return 0;

    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);

    if (id.compare("root") == 0 || id.compare(drive_id) == 0)
        return 0;

    int ret = Delete(path);
    if (ret != 0)
    {
        std::string subfolders = path + "/";
        for (std::map<std::string, std::string>::iterator it = path_id_map.begin(); it != path_id_map.end();)
        {
            if (strncmp(it->first.c_str(), subfolders.c_str(), path.length()) == 0)
            {
                it = path_id_map.erase(it);
            }
            else
                ++it;
        }
    }
    return ret;
}

int GDriveClient::Delete(const std::string &path)
{
    std::string id = GetValue(path_id_map, path);
    std::string drive_id = GetDriveId(path);
    if (strcmp(id.c_str(), "root") == 0)
        return 0;

    std::string url = std::string("/drive/v3/files/") + BaseClient::Escape(id);
    if (!drive_id.empty())
        url += "?supportsAllDrives=true";
    if (auto res = client->Delete(url))
    {
        if (HTTP_SUCCESS(res->status))
        {
            path_id_map.erase(path);
            sprintf(response, "%d", res->status);
        }
        else
            return 0;
    }
    else
    {
        sprintf(response, "%s", to_string(res.error()).c_str());
        return 0;
    }
    return 1;
}

void SetupSharedWithMeFolder(DirEntry *entry)
{
    memset(entry, 0, sizeof(DirEntry));
    sprintf(entry->directory, "%s", "/");
    sprintf(entry->name, "%s", shared_with_me.c_str());
    sprintf(entry->path, "/%s", shared_with_me.c_str());
    sprintf(entry->display_size, "%s", lang_strings[STR_FOLDER]);
    entry->file_size = 0;
    entry->isDir = true;
    entry->selectable = false;
}

void SetupMyDriveFolder(DirEntry *entry)
{
    memset(entry, 0, sizeof(DirEntry));
    sprintf(entry->directory, "%s", "/");
    sprintf(entry->name, "%s", my_drive.c_str());
    sprintf(entry->path, "/%s", my_drive.c_str());
    sprintf(entry->display_size, "%s", lang_strings[STR_FOLDER]);
    entry->file_size = 0;
    entry->isDir = true;
    entry->selectable = false;
}

std::vector<DirEntry> GDriveClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    Util::SetupPreviousFolder(path, &entry);
    out.push_back(entry);

    if (strcmp(path.c_str(), "/") == 0)
    {
        SetupSharedWithMeFolder(&entry);
        out.push_back(entry);
        SetupMyDriveFolder(&entry);
        out.push_back(entry);

        std::string drives_url = std::string("/drive/v3/drives?pageSize=100");
        if (auto res = client->Get(drives_url))
        {
            if (HTTP_SUCCESS(res->status))
            {
                json_object *jobj = json_tokener_parse(res->body.c_str());
                json_object *drives = json_object_object_get(jobj, "drives");
                if (json_object_get_type(drives) == json_type_array)
                {
                    struct array_list *adrives = json_object_get_array(drives);
                    for (size_t idx = 0; idx < adrives->length; ++idx)
                    {
                        json_object *drive = (json_object *)array_list_get_idx(adrives, idx);
                        DirEntry entry;
                        memset(&entry, 0, sizeof(DirEntry));

                        sprintf(entry.directory, "%s", path.c_str());
                        entry.selectable = false;
                        entry.file_size = 0;
                        entry.isDir = true;
                        sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);

                        const char *id = json_object_get_string(json_object_object_get(drive, "id"));
                        const char *name = json_object_get_string(json_object_object_get(drive, "name"));
                        sprintf(entry.name, "%s", name);
                        sprintf(entry.path, "/%s", name);
                        shared_drive_map.insert(std::make_pair(entry.path, id));
                        path_id_map.insert(std::make_pair(entry.path, id));
                        out.push_back(entry);
                    }
                }
            }
        }

        return out;
    }

    std::string id = GetValue(path_id_map, path);
    if (id.empty())
    {
        if (FileExists(path))
        {
            id = GetValue(path_id_map, path);
        }
        else
        {
            return out;
        }
    }

    std::string drive_id = GetDriveId(path);
    std::string base_url = std::string("/drive/v3/files?q=") + BaseClient::Escape("\"" + id + "\" in parents") +
                           "&pageSize=1000&fields=" + BaseClient::Escape("files(id,mimeType,name,modifiedTime,size),nextPageToken");
    if (!drive_id.empty())
    {
        base_url += "&driveId=" + drive_id + "&corpora=drive&includeItemsFromAllDrives=true&supportsAllDrives=true";
    }

    bool find_no_parent = false;
    if (id.compare(shared_with_me) == 0)
    {
        base_url = std::string("/drive/v3/files?q=sharedWithMe&pageSize=1000&fields=") + BaseClient::Escape("files(id,mimeType,name,modifiedTime,size),nextPageToken");
    }

    std::string next_page_url = base_url;
    while (true)
    {
        if (auto res = client->Get(next_page_url))
        {
            if (HTTP_SUCCESS(res->status))
            {
                json_object *jobj = json_tokener_parse(res->body.c_str());
                json_object *next_page_token = json_object_object_get(jobj, "nextPageToken");
                json_object *files = json_object_object_get(jobj, "files");
                if (json_object_get_type(files) == json_type_array)
                {
                    struct array_list *afiles = json_object_get_array(files);
                    for (size_t idx = 0; idx < afiles->length; ++idx)
                    {
                        json_object *file = (json_object *)array_list_get_idx(afiles, idx);
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
                if (next_page_token != nullptr)
                    next_page_url = base_url + "&pageToken=" + BaseClient::Escape(json_object_get_string(next_page_token));
                else
                    break;
            }
        }
        else
        {
            break;
        }
    };
    return out;
}

ClientType GDriveClient::clientType()
{
    return CLIENT_TYPE_GOOGLE;
}

uint32_t GDriveClient::SupportedActions()
{
    return REMOTE_ACTION_ALL ^ REMOTE_ACTION_CUT ^ REMOTE_ACTION_COPY ^ REMOTE_ACTION_PASTE ^ REMOTE_ACTION_RAW_READ;
}

void *GDriveClient::RefreshTokenThread(void *argp)
{
    while (refresh_token_running)
    {
        OrbisTick tick;
        memset(&tick, 0, sizeof(OrbisTick));
        sceRtcGetCurrentTick(&tick);
        if (tick.mytick >= (remote_settings->gg_account.token_expiry - 300000000) &&
            remote_settings->type == CLIENT_TYPE_GOOGLE) // refresh token 5mins before expiry
        {
            RefreshAccessToken();
        }
        sceKernelUsleep(500000); // check every 0.5s
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
        refresh_token_running = false;
        return;
    }
}

void GDriveClient::StopRefreshToken()
{
    refresh_token_running = false;
}

int GDriveClient::Quit()
{
    StopRefreshToken();
    if (client != nullptr)
    {
        delete client;
        client = nullptr;
    }
    return 1;
}

void GDriveClient::SetAccessToken(const std::string &token)
{
    if (this->client != nullptr)
        this->client->set_bearer_token_auth(token);
}

void *GDriveClient::Open(const std::string &path, int flags)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return nullptr;
}

void GDriveClient::Close(void *fp)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
}

int GDriveClient::GetRange(void *fp, DataSink &sink, uint64_t size, uint64_t offset)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return -1;
}