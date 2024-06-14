#ifndef EZ_RCLONE_H
#define EZ_RCLONE_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "clients/baseclient.h"
#include "clients/remote_client.h"
#include "common.h"

class RCloneClient : public BaseClient
{
public:
    std::vector<DirEntry> ListDir(const std::string &path);
};

#endif