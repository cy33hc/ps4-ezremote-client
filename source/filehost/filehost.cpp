#include <regex>
#include <string>
#include <vector>
#include <map>

#include "filehost.h"
#include "1fichier.h"
#include "filehost/alldebrid.h"
#include "filehost/directhost.h"
#include "filehost/gdrive.h"
#include "filehost/mediafire.h"
#include "filehost/pixeldrain.h"
#include "config.h"
#include "util.h"

#define GDRIVE_REGEX "https:\\/\\/drive\\.google\\.com\\/(.*)"
#define MEDIAFIRE_REGEX "https:\\/\\/www\\.mediafire\\.com\\/(.*)"
#define PIXELDRAIN_REGEX "https:\\/\\/pixeldrain\\.com\\/(.*)"
#define FICHIER_REGEX "https:\\/\\/1fichier\\.com\\/(.*)"

static std::map<std::string, std::string> cache_downloal_urls;

std::string FileHost::GetUrl()
{
    return url;
}

FileHost *FileHost::getFileHost(const std::string &url, bool use_alldebrid)
{
    std::regex google_re(GDRIVE_REGEX);
    std::regex mediafire_re(MEDIAFIRE_REGEX);
    std::regex pixeldrain_re(PIXELDRAIN_REGEX);
    std::regex fichier_re(FICHIER_REGEX);

    if (use_alldebrid)
        return new AllDebridHost(url);
    else if (std::regex_match(url, google_re))
        return new GDriveHost(url);
    else if (std::regex_match(url, mediafire_re))
        return new MediaFireHost(url);
    else if (std::regex_match(url, pixeldrain_re))
        return new PixelDrainHost(url);
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
    cache_downloal_urls.erase(hash);
    cache_downloal_urls.insert(pair);
}
