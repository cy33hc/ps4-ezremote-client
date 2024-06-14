#ifndef EZ_FICHIER_HOST_H
#define EZ_FICHIER_HOST_H

#include "filehost.h"

class FichierHost : public FileHost
{
public:
    FichierHost(const std::string &url);
    bool IsValidUrl();
    std::string GetDownloadUrl();
};

#endif