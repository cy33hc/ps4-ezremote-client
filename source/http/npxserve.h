#ifndef NPXSERVER_H
#define NPXSERVER_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "common.h"
#include "remote_client.h"

class NpxServeClient : public RemoteClient
{
public:
    NpxServeClient();
    ~NpxServeClient();
    int Connect(const std::string &url, const std::string &username, const std::string &password);
    int Mkdir(const std::string &path);
    int Rmdir(const std::string &path, bool recursive);
    int Size(const std::string &path, int64_t *size);
    int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
    int Put(const std::string &inputfile, const std::string &path, uint64_t offset=0);
    int Rename(const std::string &src, const std::string &dst);
    int Delete(const std::string &path);
    int Copy(const std::string &from, const std::string &to);
    int Move(const std::string &from, const std::string &to);
    int Head(const std::string &path, void *buffer, uint64_t len);
    bool FileExists(const std::string &path);
    std::vector<DirEntry> ListDir(const std::string &path);
    std::string GetPath(std::string path1, std::string path2);
    bool IsConnected();
    bool Ping();
    const char *LastResponse();
    int Quit();
    ClientType clientType();
    uint32_t SupportedActions();

private:
    httplib::Client *client;
    char response[512];
    bool connected = false;
};

#endif