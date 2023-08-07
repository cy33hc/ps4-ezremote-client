#include <regex>
#include <lexbor/html/parser.h>
#include <lexbor/dom/interfaces/element.h>
#include <http/httplib.h>

#include "common.h"
#include "1fichier.h"

#define VALIDATION_REGEX "https:\\/\\/1fichier\\.com\\/(.*)"

FichierHost::FichierHost(const std::string &url) : FileHost(url)
{
}

bool FichierHost::IsValidUrl()
{
    std::regex regex(VALIDATION_REGEX);

    if (std::regex_match(url, regex))
        return true;
    return false;
}

std::string FichierHost::GetDownloadUrl()
{
    std::regex re("https:\\/\\/1fichier\\.com");
    std::string path = std::regex_replace(url, re, "");

    httplib::Client tmp_client("https://1fichier.com");
    tmp_client.set_keep_alive(true);
    tmp_client.set_follow_location(true);
    tmp_client.set_connection_timeout(30);
    tmp_client.set_read_timeout(30);
    tmp_client.enable_server_certificate_verification(false);

    auto res = tmp_client.Get(path);
    if (HTTP_SUCCESS(res->status))
    {
        lxb_status_t status;
        lxb_dom_element_t *element;
        lxb_dom_node_t *node;
        lxb_html_document_t *document;
        lxb_dom_collection_t *collection;
        lxb_dom_attr_t *attr;
        const lxb_char_t *value;
        size_t value_len;
        std::string download_url = "";

        document = lxb_html_document_create();
        status = lxb_html_document_parse(document, (lxb_char_t *)res->body.c_str(), res->body.length());
        if (status != LXB_STATUS_OK)
            return "";
        collection = lxb_dom_collection_make(&document->dom_document, 128);
        if (collection == NULL)
        {
            lxb_html_document_destroy(document);
            return "";
        }

        status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                            collection, (const lxb_char_t *)"input", 5);
        if (status != LXB_STATUS_OK)
        {
            lxb_dom_collection_destroy(collection, true);
            lxb_html_document_destroy(document);
            return "";
        }

        std::string post_data;
        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++)
        {
            element = lxb_dom_collection_element(collection, i);
            value = lxb_dom_element_get_attribute(element, (const lxb_char_t *)"name", 4, &value_len);
            if (value != nullptr)
            {
                std::string name_attr((char *)value, value_len);
                if (name_attr == "adz")
                {
                    value = lxb_dom_element_get_attribute(element, (const lxb_char_t *)"value", 5, &value_len);
                    std::string adz_value((char *)value, value_len);
                    post_data = std::string("adz=") + adz_value + "&did=0&dl_no_ssl=off&dlinline=on";
                    break;
                }
            }
        }
        lxb_dom_collection_destroy(collection, true);
        lxb_html_document_destroy(document);

        if (auto res = tmp_client.Post(path, post_data.c_str(), post_data.length(), "application/x-www-form-urlencoded"))
        {
            if (HTTP_SUCCESS(res->status))
            {
                document = lxb_html_document_create();
                status = lxb_html_document_parse(document, (lxb_char_t *)res->body.c_str(), res->body.length());
                if (status != LXB_STATUS_OK)
                    return "";

                collection = lxb_dom_collection_make(&document->dom_document, 128);
                if (collection == NULL)
                {
                    lxb_html_document_destroy(document);
                    return "";
                }
                
                status = lxb_dom_elements_by_tag_name(lxb_dom_interface_element(document->body),
                                                    collection, (const lxb_char_t *)"a", 1);
                if (status != LXB_STATUS_OK)
                {
                    lxb_dom_collection_destroy(collection, true);
                    lxb_html_document_destroy(document);
                    return "";
                }

                for (size_t i = 0; i < lxb_dom_collection_length(collection); i++)
                {
                    element = lxb_dom_collection_element(collection, i);
                    value = lxb_dom_element_get_attribute(element, (const lxb_char_t *)"class", 5, &value_len);
                    if (value != nullptr)
                    {
                        std::string class_value((char*) value, value_len);
                        if (class_value == "ok btn-general btn-orange")
                        {
                            value = lxb_dom_element_get_attribute(element, (const lxb_char_t *)"href", 4, &value_len);
                            if (value != nullptr)
                            {
                                download_url = std::string((char*) value, value_len);
                                break;
                            }
                        }
                    }

                }
                lxb_dom_collection_destroy(collection, true);
                lxb_html_document_destroy(document);
            }
        }

        return download_url;
    }
    
    return "";
}
