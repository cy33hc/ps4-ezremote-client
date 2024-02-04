#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <algorithm>
#include <stdarg.h>
#include <orbis/libkernel.h>
#include "base64.h"
#include "openssl/md5.h"
#include "lang.h"

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

    static inline bool EndsWith(std::string const &value, std::string const &ending)
    {
        if (ending.size() > value.size())
            return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    static inline std::vector<std::string> Split(const std::string &str, const std::string &delimiter)
    {
        std::string text = std::string(str);
        std::vector<std::string> tokens;
        size_t pos = 0;
        while ((pos = text.find(delimiter)) != std::string::npos)
        {
            if (text.substr(0, pos).length() > 0)
                tokens.push_back(text.substr(0, pos));
            text.erase(0, pos + delimiter.length());
        }
        if (text.length() > 0)
        {
            tokens.push_back(text);
        }

        return tokens;
    }

    static inline std::string UrlHash(const std::string &text)
    {
        std::vector<unsigned char> res(16);
        MD5((const unsigned char *)text.c_str(), text.length(), res.data());

        std::string out;
        Base64::Encode(res.data(), res.size(), out);
        Util::ReplaceAll(out, "=", "_");
        Util::ReplaceAll(out, "+", "_");
        out = out + ".pkg";
        return out;
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

    static inline void SetupPreviousFolder(const std::string &path, DirEntry *entry)
    {
        memset(entry, 0, sizeof(DirEntry));
        if (path[path.length() - 1] == '/' && path.length() > 1)
        {
            strlcpy(entry->directory, path.c_str(), path.length() - 1);
        }
        else
        {
            sprintf(entry->directory, "%s", path.c_str());
        }
        sprintf(entry->name, "%s", "..");
        sprintf(entry->path, "%s", entry->directory);
        sprintf(entry->display_size, "%s", lang_strings[STR_FOLDER]);
        entry->file_size = 0;
        entry->isDir = true;
        entry->selectable = false;
    }
}
#endif
