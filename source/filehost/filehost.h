#ifndef FILEHOST_H
#define FILEHOST_H

#include <string>
#include <vector>

class FileHost
{
public:
    FileHost(const std::string &url) { this->url = url; };
    virtual ~FileHost(){};
    virtual bool IsValidUrl() = 0;
    virtual std::string GetDownloadUrl() = 0;
    std::string GetUrl();

    static FileHost *getFileHost(const std::string &url, bool use_alldebrid = false, bool use_realdebrid = false);
    static std::string GetCachedDownloadUrl(std::string &hash);
    static void AddCacheDownloadUrl(std::string &hash, std::string &url);

protected:
    std::string url;
};

#endif