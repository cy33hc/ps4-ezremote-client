#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include <string>
#include <vector>
#include "common.h"

enum ClientType
{
    CLIENT_TYPE_FTP, CLIENT_TYPE_SMB, CLIENT_TYPE_WEBDAV
};

class RemoteClient
{
public:
    RemoteClient(){};
    virtual ~RemoteClient(){};
    virtual int Connect(const std::string &url, const std::string &username, const std::string &password) = 0;
    virtual int Mkdir(const std::string &path) = 0;
    virtual int Rmdir(const std::string &path, bool recursive) = 0;
    virtual int Size(const std::string &path, int64_t *size) = 0;
    virtual int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0) = 0;
    virtual int Put(const std::string &inputfile, const std::string &path, uint64_t offset=0) = 0;
    virtual int Rename(const std::string &src, const std::string &dst) = 0;
    virtual int Delete(const std::string &path) = 0;
    virtual bool FileExists(const std::string &path) = 0;
    virtual std::vector<DirEntry> ListDir(const std::string &path) = 0;
    virtual std::string GetPath(std::string path1, std::string path2) = 0;
    virtual bool IsConnected() = 0;
    virtual bool Ping() = 0;
    virtual const char *LastResponse() = 0;
    virtual int Quit() = 0;
    virtual ClientType clientType() = 0;
};

#endif