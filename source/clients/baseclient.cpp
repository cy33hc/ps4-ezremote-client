#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <fstream>
#include <curl/curl.h>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/baseclient.h"
#include "lang.h"
#include "util.h"
#include "windows.h"

using httplib::Client;
using httplib::Headers;
using httplib::Result;

BaseClient::BaseClient(){};

BaseClient::~BaseClient()
{
    if (client != nullptr)
        delete client;
};

int BaseClient::Connect(const std::string &url, const std::string &username, const std::string &password)
{
    std::string scheme_host_port = url;
    size_t scheme_pos = url.find("://");
    size_t root_pos = url.find("/", scheme_pos + 3);
    if (root_pos != std::string::npos)
    {
        scheme_host_port = url.substr(0, root_pos);
        this->base_path = url.substr(root_pos);
    }
    client = new httplib::Client(scheme_host_port);
    if (username.length() > 0)
        client->set_basic_auth(username, password);
    client->set_keep_alive(true);
    client->set_follow_location(true);
    client->set_connection_timeout(30);
    client->set_read_timeout(30);
    client->enable_server_certificate_verification(false);
    if (Ping())
        this->connected = true;
    return 1;
}

int BaseClient::Mkdir(const std::string &path)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int BaseClient::Rmdir(const std::string &path, bool recursive)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int BaseClient::Size(const std::string &path, int64_t *size)
{
    if (auto res = client->Head(GetFullPath(path)))
    {
        std::string content_length = res->get_header_value("Content-Length");
        if (content_length.length() > 0)
            *size = atoll(content_length.c_str());
        return 1;
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
    }
    return 0;
}

int BaseClient::Get(const std::string &outputfile, const std::string &path, uint64_t offset)
{
    std::ofstream file_stream(outputfile, std::ios::binary);
    bytes_transfered = 0;
    if (auto res = client->Get(GetFullPath(path),
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

int BaseClient::Put(const std::string &inputfile, const std::string &path, uint64_t offset)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int BaseClient::Rename(const std::string &src, const std::string &dst)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int BaseClient::Delete(const std::string &path)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int BaseClient::Copy(const std::string &from, const std::string &to)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int BaseClient::Move(const std::string &from, const std::string &to)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int BaseClient::Head(const std::string &path, void *buffer, uint64_t len)
{
    char range_header[64];
    sprintf(range_header, "bytes=%lu-%lu", 0L, len - 1);
    Headers headers = {{"Range", range_header}};
    size_t bytes_read = 0;
    std::vector<char> body;
    if (auto res = client->Get(GetFullPath(path), headers,
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

bool BaseClient::FileExists(const std::string &path)
{
    int64_t file_size;
    return Size(path, &file_size);
}

std::vector<DirEntry> BaseClient::ListDir(const std::string &path)
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

    return out;
}

std::string BaseClient::GetPath(std::string ppath1, std::string ppath2)
{
    std::string path1 = ppath1;
    std::string path2 = ppath2;
    path1 = Util::Trim(Util::Trim(path1, " "), "/");
    path2 = Util::Trim(Util::Trim(path2, " "), "/");
    path1 = this->base_path + ((this->base_path.length() > 0) ? "/" : "") + path1 + "/" + path2;
    return path1;
}

std::string BaseClient::GetFullPath(std::string ppath1)
{
    std::string path1 = ppath1;
    path1 = Util::Trim(Util::Trim(path1, " "), "/");
    path1 = this->base_path + "/" + path1;
    Util::ReplaceAll(path1, "//", "/");
    return path1;
}

bool BaseClient::IsConnected()
{
    return this->connected;
}

bool BaseClient::Ping()
{
    if (auto res = client->Head("/"))
    {
        return true;
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
    }
    return false;
}

const char *BaseClient::LastResponse()
{
    return this->response;
}

int BaseClient::Quit()
{
    if (client != nullptr)
    {
        delete client;
        client = nullptr;
    }
    return 1;
}

ClientType BaseClient::clientType()
{
    return CLIENT_TYPE_HTTP_SERVER;
}

uint32_t BaseClient::SupportedActions()
{
    return REMOTE_ACTION_DOWNLOAD | REMOTE_ACTION_INSTALL;
}

std::string BaseClient::EncodeUrl(const std::string &url)
{
    CURL *curl = curl_easy_init();
    if (curl)
    {
        char *output = curl_easy_escape(curl, url.c_str(), url.length());
        if (output)
        {
            std::string encoded_url = std::string(output);
            curl_free(output);
            return encoded_url;
        }
        curl_easy_cleanup(curl);
    }
    return "";
}

std::string BaseClient::DecodeUrl(const std::string &url)
{
    CURL *curl = curl_easy_init();
    if (curl)
    {
        int decode_len;
        char *output = curl_easy_unescape(curl, url.c_str(), url.length(), &decode_len);
        if (output)
        {
            std::string decoded_url = std::string(output, decode_len);
            curl_free(output);
            return decoded_url;
        }
        curl_easy_cleanup(curl);
    }
    return "";
}
