#ifndef EZ_MEDIAFIRE_HOST_H
#define EZ_MEDIAFIRE_HOST_H

#include "filehost.h"

class MediaFireHost : public FileHost
{
public:
    MediaFireHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif