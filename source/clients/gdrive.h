#ifndef GDRIVE_H
#define GDRIVE_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "clients/remote_client.h"
#include "clients/baseclient.h"
#include "common.h"

static pthread_t refresh_token_thid;
static bool refresh_token_running = false;
extern int login_state;

class GDriveClient : public BaseClient
{
public:
    GDriveClient();
    int Connect(const std::string &url, const std::string &user, const std::string &pass);
    int Rename(const std::string &src, const std::string &dst);
    int Get(const std::string &outputfile, const std::string &path, uint64_t offset=0);
    int GetRange(const std::string &path, void *buffer, uint64_t size, uint64_t offset);
    int Put(const std::string &inputfile, const std::string &path, uint64_t offset=0);
    int Head(const std::string &path, void *buffer, uint64_t len);
    int Update(const std::string &inputfile, const std::string &path);
    int Size(const std::string &path, int64_t *size);
    int Mkdir(const std::string &path);
    int Rmdir(const std::string &path, bool recursive);
    int Delete(const std::string &path);
    bool FileExists(const std::string &path);
    void SetAccessToken(const std::string &token);
    std::vector<DirEntry> ListDir(const std::string &path);
    static void *RefreshTokenThread(void *argp);
    static void StartRefreshToken();
    static void StopRefreshToken();
    ClientType clientType();
    uint32_t SupportedActions();
    int Quit();

private:
    int RequestAuthorization();
    std::string GetDriveId(const std::string path);
    std::map<std::string, std::string> path_id_map;
    std::map<std::string, std::string> shared_drive_map;
};

#endif