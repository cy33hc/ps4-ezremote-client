#include <string>
#include <json-c/json.h>
#include "http/httplib.h"
#include "server/http_server.h"
#include "clients/gdrive.h"
#include "config.h"
#include "fs.h"
#include "windows.h"
#include "lang.h"
#include "system.h"
#include "dbglogger.h"

#define SERVER_CERT_FILE "/app0/assets/certs/domain.crt"
#define SERVER_PRIVATE_KEY_FILE "/app0/assets/certs/domain.key"
#define SERVER_PRIVATE_KEY_PASSWORD "12345678"
#define SUCCESS_MSG "{ \"result\": { \"success\": true, \"error\": null } }"
#define FAILURE_MSG "{ \"result\": { \"success\": false, \"error\": \"%s\" } }"
#define SUCCESS_MSG_LEN 48

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

    void failed(Response & res, int status, const std::string &msg)
    {
        res.status = status;
        char response_msg[msg.length()+strlen(FAILURE_MSG)+2];
        snprintf(response_msg, sizeof(response_msg), "{ \"result\": { \"success\": false, \"error\": \"%s\" } }", msg.c_str());
        res.set_content(response_msg, strlen(response_msg), "application/json");
        return;
    }

    void bad_request(Response & res, const std::string &msg)
    {
        failed(res, 200, msg);
        return;
    }

    void success(Response & res)
    {
        res.status = 200;
        res.set_content(SUCCESS_MSG, SUCCESS_MSG_LEN, "application/json");
        return;
    }

    int CopyOrMove(const DirEntry &src, const char *dest, bool isCopy)
    {
        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<DirEntry> entries = FS::ListDir(src.path, &err);
            FS::MkDirs(dest);
            for (int i = 0; i < entries.size(); i++)
            {
                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = (char *)malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    FS::MkDirs(new_path);
                    ret = CopyOrMove(entries[i], new_path, isCopy);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    if (isCopy)
                    {
                        ret = FS::Copy(entries[i].path, new_path);
                    }
                    else
                    {
                        ret = FS::Move(entries[i].path, new_path);
                    }
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
            if (!isCopy)
                FS::RmRecursive(src.path);
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = (char *)malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            if (isCopy)
            {
                ret = FS::Copy(src.path, new_path);
            }
            else
            {
                ret = FS::Move(src.path, new_path);
            }
            if (ret <= 0)
            {
                free(new_path);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    void *ServerThread(void *argp)
    {
        svr->Get("/", [&](const Request & req, Response & res)
        {
            res.set_redirect("/mnt/sandbox/pfsmnt/RMTC00001-app0/assets/index.html");
        });

        svr->Post("/__local__/list", [&](const Request & req, Response & res)
        {
            const char *path;
            bool onlyFolders = false;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                path = json_object_get_string(json_object_object_get(jobj, "path"));
                const char *onlyFolders_text = json_object_get_string(json_object_object_get(jobj, "onlyFolders"));
                if (onlyFolders_text != nullptr && strcasecmp(onlyFolders_text, "true")==0)
                    onlyFolders = true;
                if (path == nullptr)
                {
                    bad_request(res, "Required path parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            int err;
            std::vector<DirEntry> files = FS::ListDir(path, &err);
            json_object *json_files = json_object_new_array();
            for (std::vector<DirEntry>::iterator it = files.begin(); it != files.end();)
            {
                if (((onlyFolders && it->isDir) || !onlyFolders) && strcmp(it->name, "..") != 0)
                {
                    json_object *new_file = json_object_new_object();
                    json_object_object_add(new_file, "name", json_object_new_string(it->name));
                    json_object_object_add(new_file, "rights", json_object_new_string(it->isDir ? "drwxrwxrwx" : "rw-rw-rw-"));
                    json_object_object_add(new_file, "date", json_object_new_string(it->display_date));
                    json_object_object_add(new_file, "size", json_object_new_string(it->isDir ? "" : std::to_string(it->file_size).c_str()));
                    json_object_object_add(new_file, "type", json_object_new_string(it->isDir ? "dir" : "file"));
                    json_object_array_add(json_files, new_file);
                }
                it++;
            }
            json_object *results = json_object_new_object();
            json_object_object_add(results, "result", json_files);
            const char *results_str = json_object_to_json_string(results);
            res.status = 200;
            res.set_content(results_str, strlen(results_str), "application/json");
        });

        svr->Post("/__local__/rename", [&](const Request & req, Response & res)
        {
            const char *item;
            const char *newItemPath;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                item = json_object_get_string(json_object_object_get(jobj, "item"));
                newItemPath = json_object_get_string(json_object_object_get(jobj, "newItemPath"));
                if (item == nullptr || newItemPath == nullptr)
                {
                    bad_request(res, "Required item or newItemPath parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            FS::Rename(item, newItemPath);
            success(res);
            return;
        });

        svr->Post("/__local__/move", [&](const Request & req, Response & res)
        {
            const json_object *items;
            const char *newPath;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                newPath = json_object_get_string(json_object_object_get(jobj, "newPath"));
                if (items == nullptr || newPath == nullptr)
                {
                    bad_request(res, "Required items or newPath parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::string failed_items;
            size_t len = json_object_array_length(items);
            for (size_t i=0; i < len; i++)
            {
                const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                DirEntry entry;
                std::string temp = std::string(item);
                size_t slash_pos = temp.find_last_of("/");
                sprintf(entry.name, "%s", temp.substr(slash_pos+1).c_str());
                sprintf(entry.path, "%s", item);
                entry.isDir = FS::IsFolder(item);
                bool ret = CopyOrMove(entry, newPath, false);
                if (!ret)
                {
                    failed_items += std::string(item) + ",";
                }
            }

            if (failed_items.length() > 0)
            {
                std::string error_msg = std::string("One or more file(s) failed to move. ") + failed_items;
                failed(res, 200, error_msg);
            }
            else
                success(res);
        });

        svr->Post("/__local__/copy", [&](const Request & req, Response & res)
        {
            const json_object *items;
            const char *newPath;
            const char *singleFilename;

            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                newPath = json_object_get_string(json_object_object_get(jobj, "newPath"));
                singleFilename = json_object_get_string(json_object_object_get(jobj, "singleFilename"));

                if (items == nullptr || newPath == nullptr)
                {
                    bad_request(res, "Required items or newPath or singleFilename parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::string failed_items;
            if (singleFilename != nullptr)
            {
                const char *src = json_object_get_string(json_object_array_get_idx(items, 0));
                std::string dest = std::string(newPath) + "/" + singleFilename;
                if (dest.compare(src) != 0 && !FS::Copy(src, dest))
                {
                    failed_items += src;
                }
            }
            else
            {
                size_t len = json_object_array_length(items);
                for (size_t i=0; i < len; i++)
                {
                    const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                    DirEntry entry;
                    std::string temp = std::string(item);
                    size_t slash_pos = temp.find_last_of("/");
                    sprintf(entry.name, "%s", temp.substr(slash_pos+1).c_str());
                    sprintf(entry.path, "%s", item);
                    entry.isDir = FS::IsFolder(item);
                    bool ret = CopyOrMove(entry, newPath, true);
                    if (!ret)
                    {
                        failed_items += std::string(item) + ",";
                    }
                }
            }

            if (failed_items.length() > 0)
            {
                std::string error_msg = std::string("One or more file(s) failed to copy. ") + failed_items;
                failed(res, 200, error_msg);
            }
            else
                success(res);
        });

        svr->Post("/__local__/remove", [&](const Request & req, Response & res)
        {
            json_object *items;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                items = json_object_object_get(jobj, "items");
                if (items == nullptr)
                {
                    bad_request(res, "Required items parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::string failed_items;
            size_t len = json_object_array_length(items);
            for (size_t i=0; i < len; i++)
            {
                const char *item = json_object_get_string(json_object_array_get_idx(items, i));
                bool ret = FS::RmRecursive(item);
                if (!ret)
                {
                    failed_items += std::string(item) + ",";
                }
            }

            if (failed_items.length() > 0)
            {
                std::string error_msg = std::string("One or more file(s) failed to delete. ") + failed_items;
                failed(res, 200, error_msg);
            }
            else
                success(res);
        });

        svr->Post("/__local__/edit", [&](const Request & req, Response & res)
        {
            const char *item;
            const char *content;
            size_t content_len;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                item = json_object_get_string(json_object_object_get(jobj, "item"));
                json_object *content_obj = json_object_object_get(jobj, "content");
                content = json_object_get_string(content_obj);
                content_len = json_object_get_string_len(content_obj);
                if (item == nullptr || content == nullptr)
                {
                    bad_request(res, "Required item or content parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            bool ret = FS::Save(item, content, content_len);
            if (!ret)
            {
                failed(res, 200, "Failed to content to file.");
                return;
            }

            success(res);
        });

        svr->Post("/__local__/getContent", [&](const Request & req, Response & res)
        {
            const char *item;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                item = json_object_get_string(json_object_object_get(jobj, "item"));
                if (item == nullptr)
                {
                    bad_request(res, "Required item parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            std::vector<char> content = FS::Load(item);
            json_object *result = json_object_new_object();
            json_object_object_add(result, "result", json_object_new_string(content.data()));
            const char *result_str = json_object_to_json_string(result);
            res.status = 200;
            res.set_content(result_str, strlen(result_str), "application/json");
        });

        svr->Post("/__local__/createFolder", [&](const Request & req, Response & res)
        {
            const char *newPath;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                newPath = json_object_get_string(json_object_object_get(jobj, "newPath"));
                if (newPath == nullptr)
                {
                    bad_request(res, "Required newPath parameter missing");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }

            FS::MkDirs(newPath);
            success(res);
        });

        svr->Post("/__local__/permission", [&](const Request & req, Response & res)
        {
            failed(res, 200, "Operation not supported");
        });

        svr->Post("/__local__/compress", [&](const Request & req, Response & res)
        {
            failed(res, 200, "Operation not supported");
        });

        svr->Post("/__local__/extract", [&](const Request & req, Response & res)
        {
            failed(res, 200, "Operation not supported");
        });

        svr->Post("/__local__/upload", [&](const Request &req, Response &res, const ContentReader &content_reader)
        {
            MultipartFormDataItems items;
            std::string destination;
            FILE *out = nullptr;
            std::string new_file;
            content_reader(
                [&](const MultipartFormData &item)
                {
                    items.push_back(item);
                    if (item.name != "destination")
                    {
                        new_file = destination + "/" + item.filename;
                        if (out != nullptr)
                        {
                            FS::Close(out);
                        }
                        out = FS::Create(new_file);
                    }
                    return true;
                },
                [&](const char *data, size_t data_length)
                {
                    items.back().content.append(data, data_length);
                    if (items.back().name == "destination")
                    {
                        destination = items.back().content;
                    }
                    else
                    {
                        dbglogger_log("data_length=%lld", data_length);
                        FS::Write(out, data, data_length);
                    }
                    return true;
                });
            if (out != nullptr)
            {
                FS::Close(out);
            }
            success(res);
        });

        // Download multiple files as ZIP
        svr->Get("/__local__/downloadMultiple", [&](const Request & req, Response & res)
        {
            const char *path;
            json_object *jobj = json_tokener_parse(req.body.c_str());
            if (jobj != nullptr)
            {
                path = json_object_get_string(json_object_object_get(jobj, "path"));
                if (path == nullptr)
                {
                    failed(res, 200, "One or more file(s) failed to download");
                    return;
                }
            }
            else
            {
                bad_request(res, "Invalid payload");
                return;
            }
        });

        // Download single file
        svr->Get("/__local__/downloadFile", [&](const Request & req, Response & res)
        {
            std::string path = req.get_param_value("path", 0);
            if (path.empty())
            {
                bad_request(res, "Failed to download");
                return;
            }

            dbglogger_log("path=%s", path.c_str());
            res.status = 200;
        });

        svr->Get("/google_auth", [](const Request &req, Response &res)
        {
            std::string auth_code = req.get_param_value("code");
            Client client(GOOGLE_OAUTH_HOST);
            client.set_follow_location(true);
            client.enable_server_certificate_verification(false);
            
            std::string url = std::string("/token");
            std::string post_data = std::string("code=") + auth_code +
                                                "&client_id=" + gg_app.client_id +
                                                "&client_secret=" + gg_app.client_secret +
                                                "&redirect_uri=https%3A//localhost%3A" + std::to_string(http_server_port) + "/google_auth"
                                                "&grant_type=authorization_code";
                            
            if (auto result = client.Post(url, post_data.c_str(), post_data.length(),  "application/x-www-form-urlencoded"))
            {
                if (HTTP_SUCCESS(result->status))
                {
                    json_object *jobj = json_tokener_parse(result.value().body.c_str());
                    enum json_type type;
                    json_object_object_foreach(jobj, key, val)
                        {
                            if (strcmp(key, "access_token")==0)
                                snprintf(remote_settings->gg_account.access_token, 255, "%s", json_object_get_string(val));
                            else if (strcmp(key, "refresh_token")==0)
                                snprintf(remote_settings->gg_account.refresh_token, 255, "%s", json_object_get_string(val));
                            else if (strcmp(key, "expires_in")==0)
                            {
                                OrbisTick tick;
                                sceRtcGetCurrentTick(&tick);
                                remote_settings->gg_account.token_expiry = tick.mytick + (json_object_get_uint64(val)*1000000);
                            }
                        }
                    CONFIG::SaveConfig();
                    login_state = 1;
                    res.set_content(lang_strings[STR_GET_TOKEN_SUCCESS_MSG], "text/plain");
                    return;
                }
                else
                {
                    login_state = -1;
                    std::string str = std::string(lang_strings[STR_FAIL_GET_TOKEN_MSG]) + " Google";
                    res.set_content(str.c_str(), "text/plain");
                }
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

        svr->set_mount_point("/", "/");
        svr->listen("0.0.0.0", http_server_port);

        return NULL;
    }

    void Start()
    {
        if (svr == nullptr)
            svr = new SSLServer(SERVER_CERT_FILE, SERVER_PRIVATE_KEY_FILE, nullptr, nullptr, SERVER_PRIVATE_KEY_PASSWORD);
        if (!svr->is_valid())
        {
            return;
        }
        int ret = pthread_create(&http_server_thid, NULL, ServerThread, NULL);
    }

    void Stop()
    {
        if (svr != nullptr)
            svr->stop();
    }
}