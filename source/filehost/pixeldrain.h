#ifndef EZ_PIXELDRAIN_HOST_H
#define EZ_PIXELDRAIN_HOST_H

#include "filehost.h"

class PixelDrainHost : public FileHost
{
public:
    PixelDrainHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif