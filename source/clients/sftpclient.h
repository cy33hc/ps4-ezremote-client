#ifndef SFTPCLIENT_H
#define SFTPCLIENT_H

#include <libssh2.h>
#include <libssh2_sftp.h>
#include <string>
#include <vector>
#include "clients/remote_client.h"
#include "http/httplib.h"
#include "common.h"

class SFTPClient : public RemoteClient
{
public:
    SFTPClient();
    ~SFTPClient();
    int Connect(const std::string &url, const std::string &username, const std::string &password);
    int Mkdir(const std::string &path);
    int Rmdir(const std::string &path, bool recursive);
    int Rmdir(const std::string &path);
    int Size(const std::string &path, int64_t *size);
    int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
    int GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset);
    int GetRange(const std::string &path, DataSink &sink, uint64_t size, uint64_t offset);
    int GetRange(void *fp, DataSink &sink, uint64_t size, uint64_t offset);
    int Put(const std::string &inputfile, const std::string &path, uint64_t offset=0);
    int Rename(const std::string &src, const std::string &dst);
    int Delete(const std::string &path);
    int Copy(const std::string &from, const std::string &to);
    int Move(const std::string &from, const std::string &to);
    int Head(const std::string &path, void *buffer, uint64_t len);
    bool FileExists(const std::string &path);
    std::vector<DirEntry> ListDir(const std::string &path);
    void *Open(const std::string &path, int flags);
    void Close(void *fp);
    std::string GetPath(std::string path1, std::string path2);
    bool IsConnected();
    bool Ping();
    const char *LastResponse();
    int Quit();
    ClientType clientType();
    uint32_t SupportedActions();

protected:
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;
    int sock;
    char response[512];
    bool connected = false;
};

#endif