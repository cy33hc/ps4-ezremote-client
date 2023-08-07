#ifndef DIRECT_HOST_H
#define DIRECT_HOST_H

#include "filehost.h"

class DirectHost : public FileHost
{
public:
    DirectHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif