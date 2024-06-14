#ifndef EZ_REALDEBRID_HOST_H
#define EZ_REALDEBRID_HOST_H

#include "filehost.h"

class RealDebridHost : public FileHost
{
public:
    RealDebridHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif