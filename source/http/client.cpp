#include <http/client.hpp>
#include "web/callback.hpp"
#include "web/fsinfo.hpp"
#include "web/header.hpp"
#include "web/pugiext.hpp"
#include "web/request.hpp"
#include "web/urn.hpp"
#include "util.h"
#include <algorithm>
#include <thread>

namespace HTTP
{
    using Web::Data;
    using Web::Header;
    using Web::Request;
    using Web::Urn::Path;

    using progress_funptr = int (*)(void *context, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

    static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
    {
        std::string header(reinterpret_cast<char *>(buffer), size * nitems);
        dict_t *headers = (dict_t *)userdata;
        size_t seperator = header.find_first_of(":");

        if (seperator != std::string::npos)
        {
            std::string key = header.substr(0, seperator);
            key = Util::Trim(key, " ");
            key = Util::ToLower(key);
            std::string value = header.substr(seperator + 1);
            value = Util::Trim(value, " ");
            headers->erase(key);
            headers->insert(std::make_pair(key, value));
        }

        return (size * nitems);
    }

    Client::Client(const dict_t &options)
    {
        this->username = get(options, "username");
        this->password = get(options, "password");
        this->proxy_hostname = get(options, "proxy_hostname");
        this->proxy_username = get(options, "proxy_username");
        this->proxy_password = get(options, "proxy_password");
        this->cert_path = get(options, "cert_path");
        this->key_path = get(options, "key_path");
    }

    dict_t Client::options()
    {
        return dict_t{
            {"username", this->username},
            {"password", this->password},
            {"proxy_hostname", this->proxy_hostname},
            {"proxy_username", this->proxy_username},
            {"proxy_password", this->proxy_password},
            {"cert_path", this->cert_path},
            {"key_path", this->key_path},
        };
    }

    bool Client::Head(const std::string &url, dict_t *request_headers, Response &response)
    {
        return true;
    };

    bool Client::Get(const std::string &url, dict_t *request_headers, Response &Response)
    {
        return true;
    };

    bool Client::Delete(const std::string &url, void *post_data, dict_t *request_headers, Response &Response)
    {
        return true;
    };

    bool Client::Post(const std::string &url, void *post_data, dict_t *request_headers, Response &Response)
    {
        return true;
    };

    bool Client::Patch(const std::string &url, void *post_data, dict_t *request_headers, Response &Response)
    {
        return true;
    };

    bool Client::Put(const std::string &url, void *post_data, dict_t *request_headers, Response &Response)
    {
        return true;
    };

    bool Client::download(const std::string &url, const std::string &local_file,
                          progress_data_t progress_data, progress_t progress)
    {
        return true;
    };
}