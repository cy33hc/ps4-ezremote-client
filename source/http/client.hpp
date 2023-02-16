#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "web/callback.hpp"

namespace HTTP
{
    using progress_data_t = void *;

    using progress_t = int(void *context,
                           curl_off_t dltotal,
                           curl_off_t dlnow,
                           curl_off_t ultotal,
                           curl_off_t ulnow);

    using callback_t = std::function<void(bool)>;
    using dict_t = std::map<std::string, std::string>;

    auto inline get(const dict_t &options, const std::string &&name) -> std::string
    {
        auto it = options.find(name);
        if (it == options.end())
            return "";
        else
            return it->second;
    }

    struct Response
    {
        int err;
        int status_code;
        Web::Data body;
        dict_t headers;
    };

    class Client
    {
    public:
        Client(const dict_t &options);
        bool Head(const std::string &url, dict_t *request_headers, Response &response);
        bool Get(const std::string &url, dict_t *request_headers, Response &Response);
        bool Delete(const std::string &url, void *post_data, dict_t *request_headers, Response &Response);
        bool Post(const std::string &url, void *post_data, dict_t *request_headers, Response &Response);
        bool Patch(const std::string &url, void *post_data, dict_t *request_headers, Response &Response);
        bool Put(const std::string &url, void *post_data, dict_t *request_headers, Response &Response);
        bool download(const std::string &url, const std::string &local_file,
                      progress_data_t progress_data = nullptr, progress_t progress = nullptr);
        dict_t options();

    private:
        std::string username;
        std::string password;

        std::string proxy_hostname;
        std::string proxy_username;
        std::string proxy_password;

        std::string cert_path;
        std::string key_path;
    };
}
#endif