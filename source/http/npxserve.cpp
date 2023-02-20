#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <fstream>
#include "common.h"
#include "remote_client.h"
#include "http/npxserve.h"
#include "lang.h"
#include "util.h"
#include "windows.h"
#include "dbglogger.h"

using httplib::Client;
using httplib::Headers;
using httplib::Result;

NpxServeClient::NpxServeClient(){};

NpxServeClient::~NpxServeClient()
{
    if (client != nullptr)
        delete client;
};

int NpxServeClient::Connect(const std::string &url, const std::string &username, const std::string &password)
{
    client = new httplib::Client(url);
    if (username.length() > 0)
        client->set_basic_auth(username, password);
    client->set_keep_alive(true);
    client->set_follow_location(true);
    client->enable_server_certificate_verification(false);
    if (Ping())
        this->connected = true;
    return 1;
}

int NpxServeClient::Mkdir(const std::string &path)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int NpxServeClient::Rmdir(const std::string &path, bool recursive)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int NpxServeClient::Size(const std::string &path, int64_t *size)
{
    if (auto res = client->Head(path))
    {
        std::string content_length = res->get_header_value("Content-Length");
        if (content_length.length() > 0)
            *size = atoll(content_length.c_str());
        return 1;
    }
    return 0;
}

int NpxServeClient::Get(const std::string &outputfile, const std::string &path, uint64_t offset)
{
    std::ofstream file_stream(outputfile, std::ios::binary);
    bytes_transfered = 0;
    client->Get(path,
                [&](const char *data, size_t data_length)
                {
                    file_stream.write(data, data_length);
                    bytes_transfered += data_length;
                    return true;
                });
    file_stream.close();
    return 1;
}

int NpxServeClient::Put(const std::string &inputfile, const std::string &path, uint64_t offset)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int NpxServeClient::Rename(const std::string &src, const std::string &dst)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int NpxServeClient::Delete(const std::string &path)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int NpxServeClient::Copy(const std::string &from, const std::string &to)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int NpxServeClient::Move(const std::string &from, const std::string &to)
{
    sprintf(this->response, "%s", lang_strings[STR_UNSUPPORTED_OPERATION_MSG]);
    return 0;
}

int NpxServeClient::Head(const std::string &path, void *buffer, uint64_t len)
{
    char range_header[64];
    sprintf(range_header, "bytes=%lu-%lu", 0L, len-1);
    Headers headers = {{"Range", range_header}};

    std::vector<char> body;
    if (auto res = client->Get(path, headers,
                [&](const char *data, size_t data_length)
                {
                    dbglogger_log("bytes=%d", data_length);
                    body.insert(body.end(), data, data+data_length);
                    return true;
                }))
    {
        dbglogger_log("body.length=%d", body.size());
        if (body.size() < len) return 0;
        memcpy(buffer, body.data(), len);

        return 1;
    }
    return 0;
}

bool NpxServeClient::FileExists(const std::string &path)
{
    return 0;
}

std::vector<DirEntry> NpxServeClient::ListDir(const std::string &path)
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

    if (auto res = client->Get(path))
    {
        lxb_status_t status;
        lxb_dom_attr_t *attr;
        lxb_dom_element_t *element;
        lxb_html_document_t *document;
        lxb_dom_collection_t *collection;
        document = lxb_html_document_create();
        status = lxb_html_document_parse(document, (lxb_char_t *)res->body.c_str(), res->body.length());
        if (status != LXB_STATUS_OK)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }
        collection = lxb_dom_collection_make(&document->dom_document, 128);
        if (collection == NULL)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }
        status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                              collection, (const lxb_char_t *)"a", 1);
        if (status != LXB_STATUS_OK)
        {
            lxb_dom_collection_destroy(collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++)
        {
            DirEntry entry;
            std::string title, aclass;

            element = lxb_dom_collection_element(collection, i);
            attr = lxb_dom_element_attr_by_name(element, (lxb_char_t *)"title", 5);
            if (attr != nullptr)
                title = std::string((char *)attr->value->data, attr->value->length);
            attr = lxb_dom_element_attr_by_name(element, (lxb_char_t *)"class", 5);
            if (attr != nullptr)
                aclass = std::string((char *)attr->value->data, attr->value->length);

            sprintf(entry.directory, "%s", path.c_str());
            sprintf(entry.name, "%s", Util::Rtrim(title, "/").c_str());
            if (path.length() > 0 && path[path.length() - 1] == '/')
            {
                sprintf(entry.path, "%s%s", path.c_str(), entry.name);
            }
            else
            {
                sprintf(entry.path, "%s/%s", path.c_str(), entry.name);
            }

            sprintf(entry.display_date, "%s", "--");
            size_t space_pos = aclass.find(" ");
            std::string ent_type = aclass.substr(0, space_pos);

            if (ent_type.compare("folder") == 0)
            {
                dbglogger_log("in folder");
                sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
                entry.isDir = true;
                entry.selectable = true;
            }
            else if (ent_type.compare("file") == 0)
            {
                dbglogger_log("in file");
                sprintf(entry.display_size, "%s", "???B");
                entry.isDir = false;
                entry.selectable = true;
                entry.file_size = 0;
            }
            else
                continue;

            out.push_back(entry);
        }
        lxb_dom_collection_destroy(collection, true);
        lxb_html_document_destroy(document);
    }

finish:
    return out;
}

std::string NpxServeClient::GetPath(std::string ppath1, std::string ppath2)
{
    std::string path1 = ppath1;
    std::string path2 = ppath2;
    path1 = Util::Rtrim(Util::Trim(path1, " "), "/");
    path2 = Util::Rtrim(Util::Trim(path2, " "), "/");
    path1 = path1 + "/" + path2;
    return path1;
}

bool NpxServeClient::IsConnected()
{
    return this->connected;
}

bool NpxServeClient::Ping()
{
    if (auto res = client->Head("/"))
    {
        dbglogger_log("status=%d", res->status);
        return true;
    }
    else
    {
        dbglogger_log("error=%d", res.error());
    }
    return false;
}

const char *NpxServeClient::LastResponse()
{
    return this->response;
}

int NpxServeClient::Quit()
{
    if (client != nullptr)
    {
        delete client;
        client = nullptr;
    }
    return 1;
}

ClientType NpxServeClient::clientType()
{
    return CLIENT_TYPE_HTTP_SERVER;
}

uint32_t NpxServeClient::SupportedActions()
{
    return REMOTE_ACTION_DOWNLOAD | REMOTE_ACTION_INSTALL;
}
