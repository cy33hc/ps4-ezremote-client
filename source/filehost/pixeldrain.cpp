#include <regex>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <http/httplib.h>

#include "common.h"
#include "pixeldrain.h"


#define VALIDATION_REGEX "https:\\/\\/pixeldrain\\.com\\/u\\/(.*)"

PixelDrainHost::PixelDrainHost(const std::string &url) : FileHost(url)
{
}

bool PixelDrainHost::IsValidUrl()
{
    std::regex re(VALIDATION_REGEX);

    if (std::regex_match(url, re))
        return true;
    return false;
}

std::string PixelDrainHost::GetDownloadUrl()
{
    std::regex re(VALIDATION_REGEX);
    std::smatch matches;

    if(std::regex_search(url, matches, re))
    {
        if (matches.size() > 1)
        {
            return std::string("https://pixeldrain.com/api/file/") + matches[1].str() + "?download=";
        }
    }
    return "";
}
