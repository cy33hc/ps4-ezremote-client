#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <fstream>
#include <map>
#include <vector>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/nginx.h"
#include "lang.h"
#include "util.h"
#include "windows.h"

using httplib::Client;
using httplib::Headers;
using httplib::Result;

static std::map<std::string, int> months = {
    {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4}, {"May", 5}, {"Jun", 6},
    {"Jul", 7}, {"Aug", 8}, {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
};

std::vector<DirEntry> NginxClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    Util::SetupPreviousFolder(path, &entry);
    out.push_back(entry);

    if (auto res = client->Get(GetFullPath(path)))
    {
        lxb_status_t status;
        lxb_dom_attr_t *attr;
        lxb_dom_node_t *node;
        lxb_dom_element_t *element;
        lxb_html_document_t *document;
        lxb_dom_collection_t *collection;

        document = lxb_html_document_create();
        status = lxb_html_document_parse(document, (lxb_char_t *)res->body.c_str(), res->body.length());
        if (status != LXB_STATUS_OK)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }
        collection = lxb_dom_collection_make(&document->dom_document, 128);
        if (collection == NULL)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }
        status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                              collection, (const lxb_char_t *)"pre", 3);
        if (status != LXB_STATUS_OK)
        {
            lxb_dom_collection_destroy(collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        int coll_size = lxb_dom_collection_length(collection);
        if (coll_size < 1)
        {
            lxb_dom_collection_destroy(collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        element = lxb_dom_collection_element(collection, 0);
        const lxb_char_t *value;
        size_t value_len;
        std::string tmp;
        node = element->node.first_child;

        DirEntry entry;
        memset(&entry, 0, sizeof(DirEntry));
        do
        {
            if (node->type == LXB_DOM_NODE_TYPE_ELEMENT)
            {
                value = lxb_dom_element_local_name(lxb_dom_interface_element(node), &value_len);
                tmp = std::string((const char *)value, value_len);
                if (tmp.compare("a") == 0)
                {
                    value = lxb_dom_element_get_attribute(lxb_dom_interface_element(node), (const lxb_char_t *)"href", 4, &value_len);
                    tmp = std::string((const char *)value, value_len);
                    tmp = Util::Rtrim(tmp, "/");
                    tmp = BaseClient::DecodeUrl(tmp);
                    if (tmp.compare("..") != 0)
                    {
                        sprintf(entry.directory, "%s", path.c_str());
                        sprintf(entry.name, "%s", tmp.c_str());
                        if (path.length() > 0 && path[path.length() - 1] == '/')
                        {
                            sprintf(entry.path, "%s%s", path.c_str(), entry.name);
                        }
                        else
                        {
                            sprintf(entry.path, "%s/%s", path.c_str(), entry.name);
                        }
                    }
                }
            }
            else if (node->type == LXB_DOM_NODE_TYPE_TEXT)
            {
                value = lxb_dom_node_text_content(node, &value_len);
                tmp = std::string((const char *)value, value_len);
                std::vector<std::string> tokens = Util::Split(tmp, " ");
                if (tokens.size() == 3)
                {
                    tmp = Util::Trim(tokens[2], "\n");
                    tmp = Util::Trim(tmp, "\r");
                    if (tmp.compare("-") == 0)
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
                        entry.file_size = atoll(tmp.c_str());
                        DirEntry::SetDisplaySize(&entry);
                    }

                    std::vector<std::string> adate = Util::Split(tokens[0], "-");
                    if (adate.size() == 3)
                    {
                        entry.modified.day = atoi(adate[0].c_str());
                        entry.modified.month = months.find(adate[1])->second;
                        entry.modified.year = atoi(adate[2].c_str());
                    }

                    std::vector<std::string> atime = Util::Split(tokens[1], ":");
                    if (atime.size() == 2)
                    {
                        entry.modified.hours = atoi(atime[0].c_str());
                        entry.modified.minutes = atoi(atime[1].c_str());
                    }
                    out.push_back(entry);
                    memset(&entry, 0, sizeof(DirEntry));
                }
            }
            node = node->next;
        } while (node != nullptr);

        lxb_dom_collection_destroy(collection, true);
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