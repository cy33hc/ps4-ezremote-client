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
    using Web::Urn::encodeUrl;
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

    Client::Client()
    {
        this->username = "";
        this->password = "";
        this->proxy_hostname = "";
        this->proxy_username = "";
        this->proxy_password = "";
        this->cert_path = "";
        this->key_path = "";
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

    bool Client::InitRequest(const std::string &url, Web::Request &request, dict_t &request_headers, Response &response)
    {
        Header header = {};
        for (std::map<std::string, std::string>::iterator it = request_headers.begin(); it != request_headers.end(); it++)
        {
            header.append(it->first + ": " + it->second);
        }
        response.reset();
        request.set(CURLOPT_URL, encodeUrl(url, request.handle).c_str());
        request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
        request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&response.body));
        request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Web::Callback::Append::buffer));
        request.set(CURLOPT_HEADERDATA, &response.headers);
        request.set(CURLOPT_HEADERFUNCTION, header_callback);

        return true;
    }

    bool Client::Head(const std::string &url, dict_t &request_headers, Response &response)
    {
        Request request(this->options());
        InitRequest(url, request, request_headers, response);
        request.set(CURLOPT_CUSTOMREQUEST, "HEAD");
        request.set(CURLOPT_NOBODY, 1L);

        bool is_performed = request.perform();
        response.status_code = request.status_code();
        response.result = request.result();

        return is_performed;
    };

    bool Client::Get(const std::string &url, dict_t &request_headers, Response &response)
    {
        Request request(this->options());
        InitRequest(url, request, request_headers, response);
        request.set(CURLOPT_CUSTOMREQUEST, "GET");

        bool is_performed = request.perform();
        response.status_code = request.status_code();
        response.result = request.result();

        return is_performed;
    };

    bool Client::Delete(const std::string &url, dict_t &request_headers, Response &response)
    {
        Request request(this->options());
        InitRequest(url, request, request_headers, response);
        request.set(CURLOPT_CUSTOMREQUEST, "DELETE");

        bool is_performed = request.perform();
        response.status_code = request.status_code();
        response.result = request.result();

        return is_performed;
    };

    bool Client::Post(const std::string &url, Web::Data &data, dict_t &request_headers, Response &response)
    {
        Request request(this->options());
        InitRequest(url, request, request_headers, response);
        request.set(CURLOPT_POST, 1L);

        // set post informations
        request.set(CURLOPT_POSTFIELDS, data.buffer);
        request.set(CURLOPT_POSTFIELDSIZE, data.size);

        bool is_performed = request.perform();
        response.status_code = request.status_code();
        response.result = request.result();

        return is_performed;
    };

    bool Client::Put(const std::string &url, Web::Data &data, dict_t &request_headers, Response &response)
    {
        Request request(this->options());
        InitRequest(url, request, request_headers, response);
        request.set(CURLOPT_PUT, 1L);
        request.set(CURLOPT_UPLOAD, 1L);

        request.set(CURLOPT_READDATA, reinterpret_cast<size_t>(&data));
        request.set(CURLOPT_READFUNCTION, reinterpret_cast<size_t>(Web::Callback::Read::buffer));
        request.set(CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(data.size));

        bool is_performed = request.perform();
        response.status_code = request.status_code();
        response.result = request.result();

        return is_performed;
    };

    bool Client::Download(const std::string &url, const std::string &local_file, dict_t &request_headers,
                          Response &response, progress_data_t progress_data, progress_t progress)
    {
        std::ofstream file_stream(local_file, std::ios::binary);

        Request request(this->options());
        InitRequest(url, request, request_headers, response);

        request.set(CURLOPT_CUSTOMREQUEST, "GET");
        request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&file_stream));
        request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Web::Callback::Write::stream));
        if (progress != nullptr)
        {
            request.set(CURLOPT_XFERINFODATA, progress_data);
            request.set(CURLOPT_XFERINFOFUNCTION, progress);
            request.set(CURLOPT_NOPROGRESS, 0L);
        }

        bool is_performed = request.perform();
        response.status_code = request.status_code();
        response.result = request.result();

        return is_performed;
    };
}