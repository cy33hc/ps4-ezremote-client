#include <regex>
#include <http/httplib.h>

#include "common.h"
#include "directhost.h"


#define VALIDATION_REGEX "(.*)"

DirectHost::DirectHost(const std::string &url) : FileHost(url)
{
}

bool DirectHost::IsValidUrl()
{
    std::regex regex_1(VALIDATION_REGEX);

    if (std::regex_match(url, regex_1))
        return true;
    return false;
}

std::string DirectHost::GetDownloadUrl()
{   
    return url;
}
