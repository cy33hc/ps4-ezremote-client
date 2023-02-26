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
    int Connect(const std::string &url, const std::string &user, const std::string &pass);
    std::vector<DirEntry> ListDir(const std::string &path);
    static void *RefreshTokenThread(void *argp);
    static void StartRefreshToken();
    static void StopRefreshToken();
    ClientType clientType();
    uint32_t SupportedActions();
};

#endif