#ifndef EZ_ARCHIVEORG_H
#define EZ_ARCHIVEORG_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "clients/remote_client.h"
#include "clients/baseclient.h"
#include "common.h"

class ArchiveOrgClient : public BaseClient
{
public:
    int Connect(const std::string &url, const std::string &username, const std::string &password);
    std::vector<DirEntry> ListDir(const std::string &path);

private:
    int Login(const std::string &username, const std::string &password);
};

#endif