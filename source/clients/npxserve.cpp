#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <fstream>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/npxserve.h"
#include "lang.h"
#include "util.h"
#include "windows.h"

using httplib::Client;
using httplib::Headers;
using httplib::Result;

std::vector<DirEntry> NpxServeClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    Util::SetupPreviousFolder(path, &entry);
    out.push_back(entry);

    if (auto res = client->Get(GetFullPath(path)))
    {
        lxb_status_t status;
        lxb_dom_attr_t *attr;
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
                                              collection, (const lxb_char_t *)"a", 1);
        if (status != LXB_STATUS_OK)
        {
            lxb_dom_collection_destroy(collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++)
        {
            DirEntry entry;
            std::string title, aclass;
            memset(&entry.modified, 0, sizeof(DateTime));
            element = lxb_dom_collection_element(collection, i);
            attr = lxb_dom_element_attr_by_name(element, (lxb_char_t *)"title", 5);
            if (attr != nullptr)
                title = std::string((char *)attr->value->data, attr->value->length);
            attr = lxb_dom_element_attr_by_name(element, (lxb_char_t *)"class", 5);
            if (attr != nullptr)
                aclass = std::string((char *)attr->value->data, attr->value->length);

            sprintf(entry.directory, "%s", path.c_str());
            sprintf(entry.name, "%s", Util::Rtrim(title, "/").c_str());
            if (path.length() > 0 && path[path.length() - 1] == '/')
            {
                sprintf(entry.path, "%s%s", path.c_str(), entry.name);
            }
            else
            {
                sprintf(entry.path, "%s/%s", path.c_str(), entry.name);
            }

            sprintf(entry.display_date, "%s", "--");
            size_t space_pos = aclass.find(" ");
            std::string ent_type = aclass.substr(0, space_pos);

            if (ent_type.compare("folder") == 0)
            {
                sprintf(entry.display_size, "%s", lang_strings[STR_FOLDER]);
                entry.isDir = true;
                entry.selectable = true;
            }
            else if (ent_type.compare("file") == 0)
            {
                sprintf(entry.display_size, "%s", "???B");
                entry.isDir = false;
                entry.selectable = true;
                entry.file_size = 0;
            }
            else
                continue;

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