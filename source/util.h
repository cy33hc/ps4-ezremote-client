#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <orbis/libkernel.h>

namespace Util
{

    static inline std::string &Ltrim(std::string &str, std::string chars)
    {
        str.erase(0, str.find_first_not_of(chars));
        return str;
    }

    static inline std::string &Rtrim(std::string &str, std::string chars)
    {
        str.erase(str.find_last_not_of(chars) + 1);
        return str;
    }

    // trim from both ends (in place)
    static inline std::string &Trim(std::string &str, std::string chars)
    {
        return Ltrim(Rtrim(str, chars), chars);
    }

    static inline void ReplaceAll(std::string &data, std::string toSearch, std::string replaceStr)
    {
        size_t pos = data.find(toSearch);
        while (pos != std::string::npos)
        {
            data.replace(pos, toSearch.size(), replaceStr);
            pos = data.find(toSearch, pos + replaceStr.size());
        }
    }

    static inline std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });
        return s;
    }

    static inline void Notify(const char *fmt, ...)
    {
        OrbisNotificationRequest request;

        va_list args;
        va_start(args, fmt);
        vsprintf(request.message, fmt, args);
        va_end(args);

        request.type = OrbisNotificationRequestType::NotificationRequest;
        request.unk3 = 0;
        request.useIconImageUri = 0;
        request.targetId = -1;
        sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
    }

}
#endif
