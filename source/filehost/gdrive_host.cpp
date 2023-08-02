#include <regex>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <http/httplib.h>

#include "common.h"
#include "gdrive_host.h"


#define VALIDATION_REGEX_1 "https:\\/\\/drive\\.google\\.com\\/file\\/d\\/(.*)\\/(edit|view)\\?usp=(.*)"
#define VALIDATION_REGEX_2 "https:\\/\\/drive\\.google\\.com\\/uc\\?export=download&id=(.*)"

GDriveHost::GDriveHost(const std::string &url) : FileHost(url)
{
}

bool GDriveHost::IsValidUrl()
{
    std::regex regex_1(VALIDATION_REGEX_1);
    std::regex regex_2(VALIDATION_REGEX_1);

    if (std::regex_match(url, regex_1) || std::regex_match(url, regex_2))
        return true;
    return false;
}

std::string GDriveHost::GetDownloadUrl()
{
    std::regex re("https:\\/\\/drive\\.google\\.com");
    std::string path = std::regex_replace(url, re, "");

    httplib::Client tmp_client("https://drive.google.com");
    auto res = tmp_client.Head(path);
    if (HTTP_SUCCESS(res->status))
    {
        std::string content_type = res->get_header_value("Content-Type");
        if (content_type == "application/octet-stream")
            return url;
        else if (content_type != "text/html")
            return "";
    }
    else
        return "";

    res = tmp_client.Get(path);
    if (HTTP_SUCCESS(res->status))
    {
        lxb_status_t status;
        lxb_dom_element_t *element;
        lxb_html_document_t *document;
        lxb_dom_collection_t *collection;
        lxb_dom_attr_t *attr;
        document = lxb_html_document_create();
        status = lxb_html_document_parse(document, (lxb_char_t *)res->body.c_str(), res->body.length());
        if (status != LXB_STATUS_OK)
            return "";
        collection = lxb_dom_collection_make(&document->dom_document, 128);
        if (collection == NULL)
        {
            return "";
        }
        status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                            collection, (const lxb_char_t *)"form", 4);
        if (status != LXB_STATUS_OK)
            return "";
        std::string download_url;
        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++)
        {
            element = lxb_dom_collection_element(collection, i);
            std::string form_id((char *)element->attr_id->value->data, element->attr_id->value->length);
            if (form_id == "download-form")
            {
                size_t value_len;
                const lxb_char_t *value = lxb_dom_element_get_attribute(element, (const lxb_char_t *)"action", 6, &value_len);
                download_url = std::string((char *)value, value_len);
                break;
            }
        }
        lxb_dom_collection_destroy(collection, true);
        lxb_html_document_destroy(document);

        return download_url;
    }
    
    return "";
}
