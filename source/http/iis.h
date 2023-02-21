#ifndef IIS_H
#define IIS_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "http/baseclient.h"
#include "common.h"
#include "remote_client.h"

class IISClient : public BaseClient
{
public:
    std::vector<DirEntry> ListDir(const std::string &path);
};

#endif