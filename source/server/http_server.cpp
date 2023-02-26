#include <string>
#include <json-c/json.h>
#include "http/httplib.h"
#include "server/http_server.h"
#include "clients/gdrive.h"
#include "config.h"
#include "windows.h"
#include "lang.h"
#include "system.h"
#include "dbglogger.h"

#define SERVER_CERT_FILE "/app0/assets/certs/domain.crt"
#define SERVER_PRIVATE_KEY_FILE "/app0/assets/certs/domain.key"
#define SERVER_PRIVATE_KEY_PASSWORD "12345678"

using namespace httplib;
SSLServer *svr;
int http_server_port = 8080;

namespace HttpServer
{
    std::string dump_headers(const Headers &headers)
    {
        std::string s;
        char buf[BUFSIZ];

        for (auto it = headers.begin(); it != headers.end(); ++it)
        {
            const auto &x = *it;
            snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
            s += buf;
        }

        return s;
    }

    std::string log(const Request &req, const Response &res)
    {
        std::string s;
        char buf[BUFSIZ];

        s += "================================\n";

        snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(),
                 req.version.c_str(), req.path.c_str());
        s += buf;

        std::string query;
        for (auto it = req.params.begin(); it != req.params.end(); ++it)
        {
            const auto &x = *it;
            snprintf(buf, sizeof(buf), "%c%s=%s",
                     (it == req.params.begin()) ? '?' : '&', x.first.c_str(),
                     x.second.c_str());
            query += buf;
        }
        snprintf(buf, sizeof(buf), "%s\n", query.c_str());
        s += buf;

        s += dump_headers(req.headers);

        s += "--------------------------------\n";

        snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
        s += buf;
        s += dump_headers(res.headers);
        s += "\n";

        if (!res.body.empty())
        {
            s += res.body;
        }

        s += "\n";

        return s;
    }

    void *ServerThread(void *argp)
    {
        svr->Get("/google_auth", [](const Request &req, Response &res)
        {
            sprintf(gg_account.auth_code, "%s", req.get_param_value("code").c_str());
            SSLClient client(GOOGLE_OAUTH_HOST);
            client.enable_server_certificate_verification(false);
            std::string url = std::string("/token?code=") + gg_account.auth_code + "&client_id=" + gg_account.client_id + "&client_secret=" +
                    gg_account.client_secret + "&redirect_uri=https%3A//localhost%3A8080/google_auth&grant_type=authorization_code";
            Result result = client.Post(url);

            if (result.error() == Error::Success && result.value().status == 200)
            {
                json_object *jobj = json_tokener_parse(result.value().body.c_str());
                enum json_type type;
                json_object_object_foreach(jobj, key, val)
                    {
                        if (strcmp(key, "access_token")==0)
                            snprintf(gg_account.access_token, 255, "%s", json_object_get_string(val));
                        else if (strcmp(key, "refresh_token")==0)
                            snprintf(gg_account.refresh_token, 63, "%s", json_object_get_string(val));
                        else if (strcmp(key, "expires_in")==0)
                        {
                            OrbisTick tick;
                            sceRtcGetCurrentTick(&tick);
                            dbglogger_log("tick=%ld", tick.mytick);
                            gg_account.token_expiry = tick.mytick + (json_object_get_uint64(val)*1000000);
                            dbglogger_log("token_expiry=%ld", gg_account.token_expiry);
                        }
                    }
                CONFIG::SaveGoolgeAccountInfo();
                login_state = 1;
                res.set_content(lang_strings[STR_GET_TOKEN_SUCCESS_MSG], "text/plain");
                return;
            }
            login_state = -1;
            std::string str = std::string(lang_strings[STR_FAIL_GET_TOKEN_MSG]) + " Google";
            res.set_content(str.c_str(), "text/plain");
        });

        svr->Get("/stop", [&](const Request & /*req*/, Response & /*res*/)
        {
            svr->stop();
        });

        svr->set_error_handler([](const Request & /*req*/, Response &res)
        {
            const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
            char buf[BUFSIZ];
            snprintf(buf, sizeof(buf), fmt, res.status);
            res.set_content(buf, "text/html");
        });

        /*
        svr->set_logger([](const Request &req, const Response &res)
        {
            dbglogger_log("%s", log(req, res).c_str());
        });
        */

        svr->listen("127.0.0.1", http_server_port);

        return NULL;
    }

    void Start()
    {
        if (svr == nullptr)
            svr = new SSLServer(SERVER_CERT_FILE, SERVER_PRIVATE_KEY_FILE, nullptr, nullptr, SERVER_PRIVATE_KEY_PASSWORD);
        if (!svr->is_valid())
        {
            dbglogger_log("Not valid");
            return;
        }
        int ret = pthread_create(&http_server_thid, NULL, ServerThread, NULL);
        if (ret != 0)
        {
            dbglogger_log("Failed to start thread");
        }
    }

    void Stop()
    {
        if (svr != nullptr)
            svr->stop();
    }
}