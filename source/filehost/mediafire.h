#ifndef MEDIAFIRE_HOST_H
#define MEDIAFIRE_HOST_H

#include "filehost.h"

class MediaFireHost : public FileHost
{
public:
    MediaFireHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif