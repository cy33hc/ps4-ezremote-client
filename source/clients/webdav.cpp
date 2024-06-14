#include <fstream>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/webdav.h"
#include "pugixml/pugiext.hpp"
#include "fs.h"
#include "lang.h"
#include "util.h"
#include "system.h"
#include "windows.h"

using httplib::Client;
using httplib::Headers;
using httplib::Progress;
using httplib::Result;
using httplib::ContentProvider;

static const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

std::string WebDAVClient::GetHttpUrl(std::string url)
{
    std::string http_url = std::regex_replace(url, std::regex("webdav://"), "http://");
    http_url = std::regex_replace(http_url, std::regex("webdavs://"), "https://");
    return http_url;
}

int WebDAVClient::Connect(const std::string &host, const std::string &user, const std::string &pass)
{
    std::string url = GetHttpUrl(host);
    return BaseClient::Connect(url, user, pass);
}

Result WebDAVClient::PropFind(const std::string &path, int depth)
{
    Request req;
    Headers header = {{"Accept", "*/*"}, {"Depth", std::to_string(depth)}};

    req.method = "PROPFIND";
    req.path = path;
    req.headers = header;
    req.progress = Progress();

    return client->send(req);
}

std::vector<DirEntry> WebDAVClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    Util::SetupPreviousFolder(path, &entry);
    out.push_back(entry);

    std::string encoded_path = httplib::detail::encode_url(GetFullPath(path));
    if (auto res = PropFind(encoded_path, 1))
    {
        pugi::xml_document document;
        document.load_buffer(res->body.c_str(), res->body.length());
        auto multistatus = document.select_node("*[local-name()='multistatus']").node();
        auto responses = multistatus.select_nodes("*[local-name()='response']");
        for (auto response : responses)
        {
            pugi::xml_node href = response.node().select_node("*[local-name()='href']").node();
            std::string resource_path = httplib::detail::decode_url(href.first_child().value(), true);

            auto target_path_without_sep = GetFullPath(path);
            if (!target_path_without_sep.empty() && target_path_without_sep.back() == '/')
                target_path_without_sep.resize(target_path_without_sep.length() - 1);
            auto resource_path_without_sep = resource_path.erase(resource_path.find_last_not_of('/') + 1);
            size_t pos = resource_path_without_sep.find(this->host_url);
            if (pos != std::string::npos)
                resource_path_without_sep.erase(pos, this->host_url.length());

            if (resource_path_without_sep == target_path_without_sep)
                continue;

            pos = resource_path_without_sep.find_last_of('/');
            auto name = resource_path_without_sep.substr(pos + 1);
            auto propstat = response.node().select_node("*[local-name()='propstat']").node();
            auto prop = propstat.select_node("*[local-name()='prop']").node();
            std::string creation_date = prop.select_node("*[local-name()='creationdate']").node().first_child().value();
            std::string content_length = prop.select_node("*[local-name()='getcontentlength']").node().first_child().value();
            std::string m_date = prop.select_node("*[local-name()='getlastmodified']").node().first_child().value();
            std::string resource_type = prop.select_node("*[local-name()='resourcetype']").node().first_child().name();

            DirEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.selectable = true;
            sprintf(entry.directory, "%s", path.c_str());
            sprintf(entry.name, "%s", name.c_str());

            if (path.length() == 1 and path[0] == '/')
            {
                sprintf(entry.path, "%s%s", path.c_str(), name.c_str());
            }
            else
            {
                sprintf(entry.path, "%s/%s", path.c_str(), name.c_str());
            }

            entry.isDir = resource_type.find("collection") != std::string::npos;
            entry.file_size = 0;
            if (!entry.isDir)
            {
                entry.file_size = std::stoll(content_length);
                DirEntry::SetDisplaySize(&entry);
            }
            else
            {
                sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
            }

            char modified_date[32];
            char *p_char = NULL;
            sprintf(modified_date, "%s", m_date.c_str());
            p_char = strchr(modified_date, ' ');
            if (p_char)
            {
                OrbisDateTime gmt;
                OrbisDateTime lt;
                char month[5];
                sscanf(p_char, "%hd %s %hd %hd:%hd:%hd", &gmt.day, month, &gmt.year, &gmt.hour, &gmt.minute, &gmt.second);
                for (int k = 0; k < 12; k++)
                {
                    if (strcmp(month, months[k]) == 0)
                    {
                        gmt.month = k + 1;
                        break;
                    }
                }
                convertUtcToLocalTime(&gmt, &lt);
                entry.modified.day = lt.day;
                entry.modified.month = lt.month;
                entry.modified.year = lt.year;
                entry.modified.hours = lt.hour;
                entry.modified.minutes = lt.minute;
                entry.modified.seconds = lt.second;
            }
            out.push_back(entry);
        }
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
        return out;
    }

    return out;
}

int WebDAVClient::Put(const std::string &inputfile, const std::string &path, uint64_t offset)
{
    size_t bytes_remaining = FS::GetSize(inputfile);
    bytes_transfered = 0;
    sceRtcGetCurrentTick(&prev_tick);
    
    FILE* in = FS::OpenRead(inputfile);

    if (auto res = client->Put(GetFullPath(path),
        [&](size_t offset, DataSink &sink)
        {
            size_t buf_size = MIN(bytes_remaining, CPPHTTPLIB_RECV_BUFSIZ);
            char* buf = (char*) malloc(buf_size);
            FS::Seek(in, offset);

            while (bytes_remaining > 0)
            {
                size_t bytes_read = FS::Read(in, buf, buf_size);
                sink.write(buf, bytes_read);
                bytes_transfered += bytes_read;
                bytes_remaining -= bytes_read;
            }
            sink.done();
            free(buf);
            return true;
        },
        "application/octet-stream"))
    {
        if (HTTP_SUCCESS(res->status))
        {
            FS::Close(in);
            return 1;
        }
    }

    FS::Close(in);
    return 0;
}

int WebDAVClient::Mkdir(const std::string &path)
{
    Request req;
    Headers header = {{"Accept", "*/*"}, {"Connection", "Keep-Alive"}};

    req.method = "MKCOL";
    req.path = httplib::detail::encode_url(GetFullPath((path)));
    req.headers = header;
    req.progress = Progress();

    if (auto res = client->send(req))
    {
        if (HTTP_SUCCESS(res->status))
            return 1;
    }

    return 0;
}

int WebDAVClient::Rmdir(const std::string &path, bool recursive)
{
    return Delete(path);
}

int WebDAVClient::Rename(const std::string &src, const std::string &dst)
{
    return Move(src, dst);
}

int WebDAVClient::Delete(const std::string &path)
{
    Request req;
    Headers header = {{"Accept", "*/*"}, {"Connection", "Keep-Alive"}};

    req.method = "DELETE";
    req.path = httplib::detail::encode_url(GetFullPath((path)));
    req.headers = header;
    req.progress = Progress();

    if (auto res = client->send(req))
    {
        if (HTTP_SUCCESS(res->status))
            return 1;
    }
    
    return 0;

}

int WebDAVClient::Copy(const std::string &from, const std::string &to)
{
    Request req;
    Headers header = {{"Accept", "*/*"}, {"Destination", httplib::detail::encode_url(GetFullPath(to)) }};

    req.method = "COPY";
    req.path = httplib::detail::encode_url(GetFullPath(from));
    req.headers = header;
    req.progress = Progress();

    if (auto res = client->send(req))
    {
        if (HTTP_SUCCESS(res->status))
            return 1;
    }

    return 0;
}

int WebDAVClient::Move(const std::string &from, const std::string &to)
{
    Request req;
    Headers header = {{"Accept", "*/*"}, {"Destination", httplib::detail::encode_url(GetFullPath(to)) }};

    req.method = "MOVE";
    req.path = httplib::detail::encode_url(GetFullPath(from));
    req.headers = header;
    req.progress = Progress();

    if (auto res = client->send(req))
    {
        if (HTTP_SUCCESS(res->status))
            return 1;
    }

    return 0;
}

ClientType WebDAVClient::clientType()
{
    return CLIENT_TYPE_WEBDAV;
}

uint32_t WebDAVClient::SupportedActions()
{
    return REMOTE_ACTION_ALL ^ REMOTE_ACTION_RAW_READ;
}