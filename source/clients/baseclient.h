#ifndef BASESERVER_H
#define BASESERVER_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "clients/remote_client.h"
#include "common.h"

class BaseClient : public RemoteClient
{
public:
    BaseClient();
    ~BaseClient();
    int Connect(const std::string &url, const std::string &username, const std::string &password);
    int Mkdir(const std::string &path);
    int Rmdir(const std::string &path, bool recursive);
    int Size(const std::string &path, int64_t *size);
    int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
    int GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset);
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
    bool IsConnected();
    bool Ping();
    const char *LastResponse();
    int Quit();
    ClientType clientType();
    uint32_t SupportedActions();
    static std::string EncodeUrl(const std::string &url);
    static std::string DecodeUrl(const std::string &url);

protected:
    httplib::Client *client;
    std::string base_path;
    char response[512];
    bool connected = false;
};

#endif