#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/dom/interfaces/node.h>
#include <fstream>
#include <map>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/archiveorg.h"
#include "lang.h"
#include "util.h"
#include "system.h"
#include "windows.h"

using httplib::Client;
using httplib::Headers;
using httplib::Result;

struct InsensitiveCompare
{
    bool operator()(const std::string &a, const std::string &b) const
    {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
    }
};

static std::map<std::string, int> month_map = {{"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4}, {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8}, {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}};
static std::set<std::string, InsensitiveCompare> ignore_cookie_keys = {"path", "expires", "max-age", "domain", "secure"};

std::string ArchiveOrgClient::GenerateRandomId(const int len)
{
    static const char alphanum[] = "0123456789abcdef";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    
    return tmp_s;
}

int ArchiveOrgClient::Connect(const std::string &url, const std::string &username, const std::string &password)
{
    this->host_url = url;
    size_t scheme_pos = url.find("://");
    size_t root_pos = url.find("/", scheme_pos + 3);
    if (root_pos != std::string::npos)
    {
        this->host_url = url.substr(0, root_pos);
        this->base_path = url.substr(root_pos);
    }
    client = new httplib::Client(this->host_url);
    client->set_keep_alive(true);
    client->set_follow_location(true);
    client->set_connection_timeout(30);
    client->set_read_timeout(30);
    client->enable_server_certificate_verification(false);

    this->cookies = {
        {"donation-identifier", GenerateRandomId(32)},
        {"test-cookie", "1"},
        {"abtest-identifier", GenerateRandomId(32)}
    };

    if (username.length() > 0)
        return Login(username, password);
    else if (Ping())
        this->connected = true;
    return 1;
}

int ArchiveOrgClient::Login(const std::string &username, const std::string &password)
{
    std::string url = std::string("/account/login");
    Headers headers = {{ "User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:133.0) Gecko/20100101 Firefox/133.0"}};
    SetCookies(headers);

    MultipartFormDataItems items = {
        {"username", username, "", ""},
        {"password", password, "", ""},
        {"remember", "true", "", ""},
        {"referer", "https://archive.org/", "", ""},
        {"login", "true", "", ""},
        {"submit_by_js", "true", "", ""}};

    if (auto res = client->Post(url, headers, items))
    {
        if (HTTP_SUCCESS(res->status))
        {
            if (res->has_header("Set-Cookie"))
            {
                int cookies_count = res->get_header_value_count("Set-Cookie");

                for (int i = 0; i < cookies_count; i++)
                {
                    std::string cookie_str = res->get_header_value("Set-Cookie", i);

                    std::vector<std::string> cookies = Util::Split(cookie_str, ";");
                    for (std::vector<std::string>::iterator it = cookies.begin(); it != cookies.end();)
                    {
                        std::vector<std::string> cookie = Util::Split(*it, "=");
                        std::string key = Util::Trim(cookie[0], " ");
                        if (ignore_cookie_keys.find(key) == ignore_cookie_keys.end())
                        {
                            if (cookie.size() > 1)
                                this->cookies[key] = Util::Trim(cookie[1], " ");
                            else
                                this->cookies[key] = "";
                        }
                        ++it;
                    }
                }
                this->connected = true;
                return 1;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

std::vector<DirEntry> ArchiveOrgClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    Util::SetupPreviousFolder(path, &entry);
    out.push_back(entry);
    Headers headers;
    SetCookies(headers);

    std::string encoded_path = httplib::detail::encode_url(GetFullPath(path) + "/");
    if (auto res = client->Get(encoded_path, headers))
    {
        lxb_status_t status;
        lxb_dom_attr_t *attr;
        lxb_dom_element_t *table_element, *tr_element, *td_element;
        lxb_html_document_t *document;
        lxb_dom_collection_t *table_collection;
        lxb_dom_collection_t *tr_collection;
        lxb_dom_collection_t *td_collection;
        std::string tmp_string;
        const lxb_char_t *value;
        size_t value_len;

        document = lxb_html_document_create();
        status = lxb_html_document_parse(document, (lxb_char_t *)res->body.c_str(), res->body.length());
        if (status != LXB_STATUS_OK)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }

        table_collection = lxb_dom_collection_make(&document->dom_document, 1);
        if (table_collection == NULL)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }

        tr_collection = lxb_dom_collection_make(&document->dom_document, 128);
        if (tr_collection == NULL)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }

        status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                              table_collection, (const lxb_char_t *)"table", 5);
        if (status != LXB_STATUS_OK)
        {
            lxb_dom_collection_destroy(tr_collection, true);
            lxb_dom_collection_destroy(table_collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        if (lxb_dom_collection_length(table_collection) < 1)
        {
            lxb_dom_collection_destroy(tr_collection, true);
            lxb_dom_collection_destroy(table_collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        for (size_t i = 0; i < lxb_dom_collection_length(table_collection); i++)
        {
            table_element = lxb_dom_collection_element(table_collection, i);
            value = lxb_dom_element_class(table_element, &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string.compare("directory-listing-table") == 0)
                break;
            table_element = nullptr;
        }

        if (table_element == nullptr)
        {
            lxb_dom_collection_destroy(tr_collection, true);
            lxb_dom_collection_destroy(table_collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        status = lxb_dom_elements_by_tag_name(table_element,
                                              tr_collection, (const lxb_char_t *)"tr", 2);
        if (status != LXB_STATUS_OK && lxb_dom_collection_length(tr_collection) < 2)
        {
            lxb_dom_collection_destroy(tr_collection, true);
            lxb_dom_collection_destroy(table_collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        // skip row 0 , since it has the previous folder header
        for (size_t i = 2; i < lxb_dom_collection_length(tr_collection); i++)
        {
            DirEntry entry;
            std::string title, aclass;
            memset(&entry.modified, 0, sizeof(DateTime));

            tr_element = lxb_dom_collection_element(tr_collection, i);

            td_collection = lxb_dom_collection_make(&document->dom_document, 5);
            status = lxb_dom_elements_by_tag_name(tr_element,
                                                  td_collection, (const lxb_char_t *)"td", 2);
            if (status != LXB_STATUS_OK || lxb_dom_collection_length(td_collection) < 3)
            {
                lxb_dom_collection_destroy(td_collection, true);
                lxb_dom_collection_destroy(tr_collection, true);
                lxb_dom_collection_destroy(table_collection, true);
                lxb_html_document_destroy(document);
                goto finish;
            }

            // td0 contains the <a> tag
            td_element = lxb_dom_collection_element(td_collection, 0);
            lxb_dom_node_t *a_node = NextChildElement(td_element);
            // there is no a_node in protected links
            if (a_node == nullptr)
            {
                lxb_dom_collection_destroy(td_collection, true);
                continue;
            }

            value = lxb_dom_element_local_name(lxb_dom_interface_element(a_node), &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string.compare("a") != 0)
            {
                lxb_dom_collection_destroy(td_collection, true);
                lxb_dom_collection_destroy(tr_collection, true);
                lxb_dom_collection_destroy(table_collection, true);
                lxb_html_document_destroy(document);
                goto finish;
            }
            value = lxb_dom_element_get_attribute(lxb_dom_interface_element(a_node), (const lxb_char_t *)"href", 4, &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string[tmp_string.length()-1] == '/')
                tmp_string = tmp_string.substr(0, tmp_string.length()-1);
            tmp_string = BaseClient::UnEscape(tmp_string);
            sprintf(entry.name, "%s", tmp_string.c_str());
            sprintf(entry.directory, "%s", path.c_str());
            if (path.length() > 0 && path[path.length() - 1] == '/')
            {
                sprintf(entry.path, "%s%s", path.c_str(), entry.name);
            }
            else
            {
                sprintf(entry.path, "%s/%s", path.c_str(), entry.name);
            }

            // next td contains the date
            td_element = lxb_dom_collection_element(td_collection, 1);
            value = lxb_dom_node_text_content(NextChildTextNode(td_element), &value_len);
            tmp_string = std::string((const char *)value, value_len);
            std::vector<std::string> date_time = Util::Split(tmp_string, " ");

            if (date_time.size() > 1)
            {
                std::vector<std::string> adate = Util::Split(date_time[0], "-");
                if (adate.size() == 3)
                {
                    entry.modified.day = atoi(adate[0].c_str());
                    entry.modified.month = month_map[adate[1]];
                    entry.modified.year = atoi(adate[2].c_str());
                }

                std::vector<std::string> atime = Util::Split(date_time[1], ":");
                if (atime.size() == 2)
                {
                    entry.modified.hours = atoi(atime[0].c_str());
                    entry.modified.minutes = atoi(atime[1].c_str());
                }
            }

            // next td contains file size, if fize size is "-", then it's a directory
            td_element = lxb_dom_collection_element(td_collection, 2);
            value = lxb_dom_node_text_content(NextChildTextNode(td_element), &value_len);
            tmp_string = std::string((const char *)value, value_len);

            if (tmp_string.compare("-") == 0)
            {
                entry.isDir = true;
                entry.selectable = true;
                entry.file_size = 0;
                sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
            }
            else
            {
                entry.isDir = false;
                entry.selectable = true;
                uint64_t multiplier = 0;
                std::string size_formatted = tmp_string.substr(0, tmp_string.size()-1);
                Util::ReplaceAll(size_formatted, ",", "");
                float fsize = std::stof(size_formatted);
                switch (tmp_string[tmp_string.size()-1]) {
                    case 'B':
                        multiplier = 1;
                        break;
                    case 'K':
                        multiplier = 1024;
                        break;
                    case 'M':
                        multiplier = 1048576;
                        break;
                    case 'G':
                        multiplier = 1073741824;
                        break;
                    default:
                        multiplier = 1;
                }
                entry.file_size = fsize * multiplier;
                DirEntry::SetDisplaySize(&entry);
            }

            lxb_dom_collection_destroy(td_collection, true);
            out.push_back(entry);
        }

        lxb_dom_collection_destroy(tr_collection, true);
        lxb_dom_collection_destroy(table_collection, true);
        lxb_html_document_destroy(document);
    }
    else
    {
        sprintf(this->response, "%s", httplib::to_string(res.error()).c_str());
        return out;
    }

finish:
    return out;
}
