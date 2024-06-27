#ifndef EZ_BASESERVER_H
#define EZ_BASESERVER_H

#include <string>
#include <vector>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include "http/httplib.h"
#include "clients/remote_client.h"
#include "http/httplib.h"
#include "split_file.h"
#include "common.h"

class BaseClient : public RemoteClient
{
public:
    BaseClient();
    ~BaseClient();
    int Connect(const std::string &url, const std::string &username, const std::string &password);
    int Connect(const std::string &url, const std::string &api_key);
    int Mkdir(const std::string &path);
    int Rmdir(const std::string &path, bool recursive);
    int Size(const std::string &path, int64_t *size);
    int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
    int Get(SplitFile *split_file, const std::string &path, uint64_t offset=0);
    int GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset);
    int GetRange(const std::string &path, DataSink &sink, uint64_t size, uint64_t offset);
    int GetRange(void *fp, void *buffer, uint64_t size, uint64_t offset);
    int GetRange(void *fp, DataSink &sink, uint64_t size, uint64_t offset);
    int Put(const std::string &inputfile, const std::string &path, uint64_t offset=0);
    int Rename(const std::string &src, const std::string &dst);
    int Delete(const std::string &path);
    int Copy(const std::string &from, const std::string &to);
    int Move(const std::string &from, const std::string &to);
    int Head(const std::string &path, void *buffer, uint64_t len);
    bool FileExists(const std::string &path);
    std::vector<DirEntry> ListDir(const std::string &path);
    std::string GetPath(std::string path1, std::string path2);
    std::string GetFullPath(std::string path1);
    void *Open(const std::string &path, int flags);
    void Close(void *fp);
    bool IsConnected();
    bool Ping();
    const char *LastResponse();
    int Quit();
    ClientType clientType();
    uint32_t SupportedActions();
    static std::string Escape(const std::string &url);
    static std::string UnEscape(const std::string &url);

protected:
    httplib::Client *client;
    std::string base_path;
    std::string host_url;
    char response[512];
    bool connected = false;
};

#endif