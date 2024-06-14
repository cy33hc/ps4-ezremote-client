#ifndef EZ_GDRIVE_HOST_H
#define EZ_GDRIVE_HOST_H

#include "filehost.h"

class GDriveHost : public FileHost
{
public:
    GDriveHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif