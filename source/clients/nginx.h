#ifndef NGINX_H
#define NGINX_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "clients/baseclient.h"
#include "common.h"
#include "clients/remote_client.h"

class NginxClient : public BaseClient
{
public:
    std::vector<DirEntry> ListDir(const std::string &path);
};

#endif