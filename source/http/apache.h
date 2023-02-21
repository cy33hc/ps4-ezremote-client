#ifndef APACHE_H
#define APACHE_H

#include <string>
#include <vector>
#include "http/httplib.h"
#include "http/baseclient.h"
#include "common.h"
#include "remote_client.h"

class ApacheClient : public BaseClient
{
public:
    std::vector<DirEntry> ListDir(const std::string &path);
};

#endif