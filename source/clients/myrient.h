#ifndef EZ_MYRIENT_H
#define EZ_MYRIENT_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "clients/remote_client.h"
#include "clients/baseclient.h"
#include "common.h"

class MyrientClient : public BaseClient
{
public:
    std::vector<DirEntry> ListDir(const std::string &path);
};

#endif