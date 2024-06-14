#ifndef EZ_ALLDEBRID_HOST_H
#define EZ_ALLDEBRID_HOST_H

#include "filehost.h"

class AllDebridHost : public FileHost
{
public:
    AllDebridHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif