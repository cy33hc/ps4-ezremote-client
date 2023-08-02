#ifndef FILEHOST_H
#define FILEHOST_H

#include <string>
#include <vector>
#include "openssl/md5.h"

class FileHost
{
public:
    FileHost(const std::string &url) { this->url = url; };
    virtual ~FileHost(){};
    virtual bool IsValidUrl() = 0;
    virtual std::string GetDownloadUrl() = 0;

    std::vector<unsigned char> Hash()
    {
        std::vector<unsigned char> res(16);
        MD5((const unsigned char *)this->url.c_str(), this->url.length(), res.data());
        return res;
    }

protected:
    std::string url;
};

#endif