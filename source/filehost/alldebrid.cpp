#include <http/httplib.h>
#include <json-c/json.h>

#include "config.h"
#include "common.h"
#include "alldebrid.h"

AllDebridHost::AllDebridHost(const std::string &url) : FileHost(url)
{
}

bool AllDebridHost::IsValidUrl()
{
    httplib::Client tmp_client("https://api.alldebrid.com");
    tmp_client.set_keep_alive(true);
    tmp_client.set_follow_location(true);
    tmp_client.set_connection_timeout(30);
    tmp_client.set_read_timeout(30);
    tmp_client.enable_server_certificate_verification(false);

    std::string path = std::string("/v4/link/unlock?agent=ezRemoteClient&apikey=") + alldebrid_api_key +  "&link=" + httplib::detail::encode_url(url);
    auto res = tmp_client.Get(path);
    if (HTTP_SUCCESS(res->status))
    {
        json_object *jobj = json_tokener_parse(res->body.c_str());
        const char *status = json_object_get_string(json_object_object_get(jobj, "status"));

        if (strcmp(status, "success") == 0)
            return true;
    }
    
    return false;
}

std::string AllDebridHost::GetDownloadUrl()
{
    httplib::Client tmp_client("https://api.alldebrid.com");
    tmp_client.set_keep_alive(true);
    tmp_client.set_follow_location(true);
    tmp_client.set_connection_timeout(30);
    tmp_client.set_read_timeout(30);
    tmp_client.enable_server_certificate_verification(false);

    std::string path = std::string("/v4/link/unlock?agent=ezRemoteClient&apikey=") + alldebrid_api_key +  "&link=" + httplib::detail::encode_url(url);
    auto res = tmp_client.Get(path);
    if (HTTP_SUCCESS(res->status))
    {
        json_object *jobj = json_tokener_parse(res->body.c_str());
        const char *status = json_object_get_string(json_object_object_get(jobj, "status"));

        if (status != nullptr && strcmp(status, "success") == 0)
        {
            json_object *data = json_object_object_get(jobj, "data");
            const char *link = json_object_get_string(json_object_object_get(data, "link"));
            return std::string(link);
        }
        else
        {
            return "";
        }
    }
    
    return "";
}
