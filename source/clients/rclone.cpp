#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <fstream>
#include "common.h"
#include "clients/remote_client.h"
#include "clients/rclone.h"
#include "lang.h"
#include "util.h"
#include "system.h"
#include "windows.h"

using httplib::Client;
using httplib::Headers;
using httplib::Result;

std::vector<DirEntry> RCloneClient::ListDir(const std::string &path)
{
    std::vector<DirEntry> out;
    DirEntry entry;
    Util::SetupPreviousFolder(path, &entry);
    out.push_back(entry);

    std::string encoded_path = httplib::detail::encode_url(GetFullPath(path)+"/");
    if (auto res = client->Get(encoded_path))
    {
        lxb_status_t status;
        lxb_dom_attr_t *attr;
        lxb_dom_element_t *tbody_element, *tr_element, *td_element;
        lxb_html_document_t *document;
        lxb_dom_collection_t *tbody_collection;
        lxb_dom_collection_t *tr_collection;
        lxb_dom_collection_t *td_collection;

        document = lxb_html_document_create();
        status = lxb_html_document_parse(document, (lxb_char_t *)res->body.c_str(), res->body.length());
        if (status != LXB_STATUS_OK)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }

        tbody_collection = lxb_dom_collection_make(&document->dom_document, 1);
        if (tbody_collection == NULL)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }

        tr_collection = lxb_dom_collection_make(&document->dom_document, 128);
        if (tbody_collection == NULL)
        {
            lxb_html_document_destroy(document);
            goto finish;
        }

        status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                              tbody_collection, (const lxb_char_t *)"tbody", 5);
        if (status != LXB_STATUS_OK)
        {
            lxb_dom_collection_destroy(tr_collection, true);
            lxb_dom_collection_destroy(tbody_collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        if (lxb_dom_collection_length(tbody_collection) < 1)
        {
            lxb_dom_collection_destroy(tr_collection, true);
            lxb_dom_collection_destroy(tbody_collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        // Get the first tbody which should only be 1
        tbody_element = lxb_dom_collection_element(tbody_collection, 0);
        status = lxb_dom_elements_by_tag_name(tbody_element,
                                              tr_collection, (const lxb_char_t *)"tr", 2);
        if (status != LXB_STATUS_OK)
        {
            lxb_dom_collection_destroy(tr_collection, true);
            lxb_dom_collection_destroy(tbody_collection, true);
            lxb_html_document_destroy(document);
            goto finish;
        }

        // skip row 0 , since it has the previous folder header
        for (size_t i = 1; i < lxb_dom_collection_length(tr_collection); i++)
        {
            DirEntry entry;
            std::string title, aclass;
            memset(&entry.modified, 0, sizeof(DateTime));
            const lxb_char_t *value;
            size_t value_len;
            std::string tmp_string;

            tr_element = lxb_dom_collection_element(tr_collection, i);

            td_collection = lxb_dom_collection_make(&document->dom_document, 5);
            status = lxb_dom_elements_by_tag_name(tr_element,
                                                td_collection, (const lxb_char_t *)"td", 2);
            if (status != LXB_STATUS_OK || lxb_dom_collection_length(td_collection) < 0)
            {
                lxb_dom_collection_destroy(td_collection, true);
                lxb_dom_collection_destroy(tr_collection, true);
                lxb_dom_collection_destroy(tbody_collection, true);
                lxb_html_document_destroy(document);
                goto finish;
            }

            // td 0 is empty, td 1 is file or folder
            td_element = lxb_dom_collection_element(td_collection, 1);
            lxb_dom_node_t *use_node = NextChildElement(lxb_dom_interface_element(NextChildElement(td_element)));
            value = lxb_dom_element_local_name(lxb_dom_interface_element(use_node), &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string.compare("use") != 0)
            {
                lxb_dom_collection_destroy(td_collection, true);
                lxb_dom_collection_destroy(tr_collection, true);
                lxb_dom_collection_destroy(tbody_collection, true);
                lxb_html_document_destroy(document);
                goto finish;
            }
            value = lxb_dom_element_get_attribute(lxb_dom_interface_element(use_node), (const lxb_char_t *)"xlink:href", 10, &value_len);
            tmp_string = std::string((const char *)value, value_len);
            if (tmp_string.compare("#folder") == 0)
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

            // <a> element contains the file/folder name
            lxb_dom_node_t *a_node = NextChildElement(lxb_dom_interface_element(NextElement(NextChildElement(td_element))));
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

            // td 3 - filesize
            if (!entry.isDir)
            {
                td_element = lxb_dom_collection_element(td_collection, 2);
                lxb_dom_node_t *size_node = NextChildElement(td_element);
                value = lxb_dom_node_text_content(size_node->first_child, &value_len);
                tmp_string = std::string((const char *)value, value_len);
                entry.file_size = atol(tmp_string.c_str());
                DirEntry::SetDisplaySize(&entry);
            }

            // td 4 - datetime
            td_element = lxb_dom_collection_element(td_collection, 3);
            lxb_dom_node_t *date_node = NextChildElement(td_element);
            value = lxb_dom_element_get_attribute(lxb_dom_interface_element(date_node), (const lxb_char_t *)"datetime", 8, &value_len);
            tmp_string = std::string((const char *)value, value_len);
            std::vector<std::string> date_time = Util::Split(tmp_string, " ");

            OrbisDateTime gmt;
            OrbisDateTime lt;

            if (date_time.size() > 1)
            {
                std::vector<std::string> adate = Util::Split(date_time[0], "-");
                if (adate.size() == 3)
                {
                    gmt.year = atoi(adate[0].c_str());
                    gmt.month = atoi(adate[1].c_str());
                    gmt.day = atoi(adate[2].c_str());
                }

                std::vector<std::string> atime = Util::Split(date_time[1], ":");
                if (atime.size() == 3)
                {
                    gmt.hour = atoi(atime[0].c_str());
                    gmt.minute = atoi(atime[1].c_str());

                    std::vector<std::string> sec_msec = Util::Split(atime[2], ".");
                    if (sec_msec.size() > 0)
                    {
                        gmt.second = atoi(sec_msec[0].c_str());
                    }
                }
            }
            convertUtcToLocalTime(&gmt, &lt);
            entry.modified.day = lt.day;
            entry.modified.month = lt.month;
            entry.modified.year = lt.year;
            entry.modified.hours = lt.hour;
            entry.modified.minutes = lt.minute;
            entry.modified.seconds = lt.second;

            lxb_dom_collection_destroy(td_collection, true);
            out.push_back(entry);
        }

        lxb_dom_collection_destroy(tr_collection, true);
        lxb_dom_collection_destroy(tbody_collection, true);
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