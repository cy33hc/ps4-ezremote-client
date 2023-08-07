#include <regex>
#include <string>
#include <vector>
#include <map>
#include "openssl/md5.h"

#include "filehost.h"
#include "1fichier.h"
#include "filehost/directhost.h"
#include "filehost/gdrive.h"
#include "filehost/mediafire.h"
#include "filehost/pixeldrain.h"
#include "base64.h"
#include "util.h"

#define GDRIVE_REGEX "https:\\/\\/drive\\.google\\.com\\/(.*)"
#define MEDIAFIRE_REGEX "https:\\/\\/www\\.mediafire\\.com\\/(.*)"
#define PIXELDRAIN_REGEX "https:\\/\\/pixeldrain\\.com\\/(.*)"
#define FICHIER_REGEX "https:\\/\\/1fichier\\.com\\/(.*)"

static std::map<std::string, std::string> cache_downloal_urls;

std::string FileHost::Hash()
{
    std::vector<unsigned char> res(16);
    MD5((const unsigned char *)this->url.c_str(), this->url.length(), res.data());

    std::string out;
    Base64::Encode(res.data(), res.size(), out);
    Util::ReplaceAll(out, "=", "_");
    Util::ReplaceAll(out, "+", "_");
    out = out + ".pkg";
    return out;
}

FileHost *FileHost::getFileHost(const std::string &url)
{
    std::regex google_re(GDRIVE_REGEX);
    std::regex mediafire_re(MEDIAFIRE_REGEX);
    std::regex pixeldrain_re(PIXELDRAIN_REGEX);
    std::regex fichier_re(FICHIER_REGEX);

    if (std::regex_match(url, google_re))
        return new GDriveHost(url);
    else if (std::regex_match(url, mediafire_re))
        return new MediaFireHost(url);
    else if (std::regex_match(url, pixeldrain_re))
        return new PixelDrainHost(url);
    else if (std::regex_match(url, fichier_re))
        return new FichierHost(url);
    else
        return new DirectHost(url);
}

std::string FileHost::GetCachedDownloadUrl(std::string &hash)
{
    return cache_downloal_urls[hash];
}

void FileHost::AddCacheDownloadUrl(std::string &hash, std::string &url)
{
    std::pair<std::string, std::string> pair = std::make_pair(hash, url);
    cache_downloal_urls.insert(pair);
}
