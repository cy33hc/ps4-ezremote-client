#include <http/httplib.h>
#include <json-c/json.h>

#include "config.h"
#include "common.h"
#include "realdebrid.h"

RealDebridHost::RealDebridHost(const std::string &url) : FileHost(url)
{
}

bool RealDebridHost::IsValidUrl()
{
    httplib::Client tmp_client("https://api.real-debrid.com");
    tmp_client.set_keep_alive(true);
    tmp_client.set_follow_location(true);
    tmp_client.set_connection_timeout(30);
    tmp_client.set_read_timeout(30);
    tmp_client.enable_server_certificate_verification(false);
    tmp_client.set_bearer_token_auth(realdebrid_api_key);

    std::string path = std::string("/rest/1.0/unrestrict/check");
    std::string post_data = std::string("link=") + httplib::detail::encode_url(this->url) + "&password=";

    auto res = tmp_client.Post(path, post_data.c_str(), post_data.length(), "application/x-www-form-urlencoded");
    if (HTTP_SUCCESS(res->status))
    {
        json_object *jobj = json_tokener_parse(res->body.c_str());
        uint64_t supported = json_object_get_uint64(json_object_object_get(jobj, "supported"));

        if (supported == 1)
            return true;
    }
    
    return false;
}

std::string RealDebridHost::GetDownloadUrl()
{
    httplib::Client tmp_client("https://api.real-debrid.com");
    tmp_client.set_keep_alive(true);
    tmp_client.set_follow_location(true);
    tmp_client.set_connection_timeout(30);
    tmp_client.set_read_timeout(30);
    tmp_client.enable_server_certificate_verification(false);
    tmp_client.set_bearer_token_auth(realdebrid_api_key);

    std::string path = std::string("/rest/1.0/unrestrict/link");
    std::string post_data = std::string("link=") + httplib::detail::encode_url(this->url) + "&password=&remote=0";

    auto res = tmp_client.Post(path, post_data.c_str(), post_data.length(), "application/x-www-form-urlencoded");
    if (HTTP_SUCCESS(res->status))
    {
        json_object *jobj = json_tokener_parse(res->body.c_str());
        const char *download = json_object_get_string(json_object_object_get(jobj, "download"));

        if (download != nullptr)
        {
            return std::string(download);
        }
        else
        {
            return "";
        }
    }
    
    return "";
}
