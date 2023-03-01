#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <fstream>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/apache.h"
#include "lang.h"
#include "util.h"
#include "windows.h"

using httplib::Client;
using httplib::Headers;
using httplib::Result;

lxb_dom_node_t *nextChildElement(lxb_dom_element_t *element)
{
    lxb_dom_node_t *node = element->node.first_child;
    while (node != nullptr && node->type != LXB_DOM_NODE_TYPE_ELEMENT)
    {
        node = node->next;
    }
    return node;
}

lxb_dom_node_t *nextElement(lxb_dom_node_t *node)
{
    lxb_dom_node_t *next = node->next;
    while (next != nullptr && next->type != LXB_DOM_NODE_TYPE_ELEMENT)
    {
        next = next->next;
    }
    return next;
}

std::vector<DirEntry> ApacheClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    memset(&entry, 0, sizeof(DirEntry));
    if (path[path.length() - 1] == '/' && path.length() > 1)
    {
        strlcpy(entry.directory, path.c_str(), path.length() - 1);
    }
    else
    {
        sprintf(entry.directory, "%s", path.c_str());
    }
    sprintf(entry.name, "..");
    sprintf(entry.path, "%s", entry.directory);
    sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
    entry.file_size = 0;
    entry.isDir = true;
    entry.selectable = false;
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
                                              collection, (const lxb_char_t *)"tr", 2);
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

        const lxb_char_t *value;
        size_t value_len;
        std::string tmp_string;
        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++)
        {
            DirEntry entry;
            memset(&entry, 0, sizeof(DirEntry));

            element = lxb_dom_collection_element(collection, i);
            node = nextChildElement(element);
            if (node == nullptr) continue;

            value = lxb_dom_element_local_name(lxb_dom_interface_element(node), &value_len);
            tmp_string = std::string((const char *)value, value_len);

            if (tmp_string.compare("th") == 0)
                continue; // skip th, which are the headers

            // file/folder indicator
            if (tmp_string.compare("td") == 0)
            {
                // get the child img element
                lxb_dom_node_t *img = nextChildElement(lxb_dom_interface_element(node));
                if (img == nullptr) continue;

                value = lxb_dom_element_local_name(lxb_dom_interface_element(img), &value_len);
                tmp_string = std::string((const char *)value, value_len);
                if (tmp_string.compare("img") == 0)
                {
                    value = lxb_dom_element_get_attribute(lxb_dom_interface_element(img), (const lxb_char_t *)"alt", 3, &value_len);
                    tmp_string = std::string((const char *)value, value_len);
                    if (tmp_string.compare("[PARENTDIR]") == 0)
                        continue;
                    else if (tmp_string.compare("[DIR]") == 0)
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
                    }
                } else continue; // invalid record
            }
            else continue; // invalid record

            // file/folder name
            node = nextElement(node);
            if (node == nullptr) continue;
            value = lxb_dom_element_local_name(lxb_dom_interface_element(node), &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string.compare("td") == 0)
            {
                value = lxb_dom_node_text_content(node, &value_len);
                tmp_string = std::string((const char *)value, value_len);
                tmp_string = Util::Rtrim(tmp_string, "/");
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
            }
            else continue; // not valid record

            // datetime
            node = nextElement(node);
            if (node == nullptr) continue;
            value = lxb_dom_element_local_name(lxb_dom_interface_element(node), &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string.compare("td") == 0)
            {
                value = lxb_dom_node_text_content(node, &value_len);
                tmp_string = std::string((const char *)value, value_len);
                std::vector<std::string> date_time = Util::Split(tmp_string, " ");
                if (date_time.size() == 2)
                {
                    std::vector<std::string> adate = Util::Split(date_time[0], "-");
                    if (adate.size() == 3)
                    {
                        entry.modified.year = atoi(adate[0].c_str());
                        entry.modified.month = atoi(adate[1].c_str());
                        entry.modified.day = atoi(adate[2].c_str());
                    }

                    std::vector<std::string> atime = Util::Split(date_time[1], ":");
                    if (atime.size() == 2)
                    {
                        entry.modified.hours = atoi(atime[0].c_str());
                        entry.modified.minutes = atoi(atime[1].c_str());
                    }
                }
            }
            else continue; // invalid record

            // filesize
            node = nextElement(node);
            if (node == nullptr) continue;
            value = lxb_dom_element_local_name(lxb_dom_interface_element(node), &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string.compare("td") == 0)
            {
                value = lxb_dom_node_text_content(node, &value_len);
                tmp_string = std::string((const char *)value, value_len);
                tmp_string = Util::Trim(tmp_string, " ");
                if (!entry.isDir)
                {
                    char multiplier = tmp_string[tmp_string.length()-1];
                    std::string filesize = tmp_string.substr(0, tmp_string.length()-1);
                    sprintf(entry.display_size, "%s", tmp_string.c_str());
                    if (multiplier == 'K')
                        entry.file_size = atof(filesize.c_str()) * 1024;
                    else if (multiplier == 'M')
                        entry.file_size = atof(filesize.c_str()) * 1024 * 1024;
                    else if (multiplier == 'G')
                        entry.file_size = atof(filesize.c_str()) * 1024 * 1024 * 1024;
                    else if (multiplier == 'G')
                        entry.file_size = atof(filesize.c_str()) * 1024 * 1024 * 1024 * 1024;
                    else
                        entry.file_size = atoi(tmp_string.c_str());
                }
            }

            out.push_back(entry);
        }

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