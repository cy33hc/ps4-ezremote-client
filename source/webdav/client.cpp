/*#***************************************************************************
#                         __    __   _____       _____
#   Project              |  |  |  | |     \     /  ___|
#                        |  |__|  | |  |\  \   /  /
#                        |        | |  | )  ) (  (
#                        |   /\   | |  |/  /   \  \___
#                         \_/  \_/  |_____/     \_____|
#
# Copyright (C) 2018, The WDC Project, <rusdevops@gmail.com>, et al.
#
# This software is licensed as described in the file LICENSE, which
# you should have received as part of this distribution.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the LICENSE file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
############################################################################*/

#include <client.hpp>

#include "callback.hpp"
#include "fsinfo.hpp"
#include "header.hpp"
#include "pugiext.hpp"
#include "request.hpp"
#include "urn.hpp"
#include "util.h"
#include <algorithm>
#include <thread>

namespace WebDAV
{
  using Urn::Path;

  using progress_funptr = int (*)(void *context, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

  static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
  {
    std::string header(reinterpret_cast<char *>(buffer), size * nitems);
    dict_t *headers = (dict_t *)userdata;
    size_t seperator = header.find_first_of(":");

    if (seperator != std::string::npos)
    {
      std::string key = header.substr(0, seperator);
      key = Util::Trim(key, " ");
      key = Util::ToLower(key);
      std::string value = header.substr(seperator + 1);
      value = Util::Trim(value, " ");
      headers->erase(key);
      headers->insert(std::make_pair(key, value));
    }

    return (size * nitems);
  }

  dict_t
  Client::options()
  {
    return dict_t{
        {"webdav_hostname", this->webdav_hostname},
        {"webdav_root", this->webdav_root},
        {"webdav_username", this->webdav_username},
        {"webdav_password", this->webdav_password},
        {"proxy_hostname", this->proxy_hostname},
        {"proxy_username", this->proxy_username},
        {"proxy_password", this->proxy_password},
        {"cert_path", this->cert_path},
        {"key_path", this->key_path},
    };
  }

  long
  Client::status_code()
  {
    return this->http_code;
  }

  bool
  Client::sync_download(
      const std::string &remote_file,
      const std::string &local_file,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    bool is_existed = this->check(remote_file);
    if (!is_existed)
      return false;

    auto root_urn = Path(this->webdav_root, true);
    auto file_urn = root_urn + remote_file;

    std::ofstream file_stream(local_file, std::ios::binary);

    Request request(this->options());

    auto url = this->webdav_hostname + file_urn.quote(request.handle);
    request.set(CURLOPT_CUSTOMREQUEST, "GET");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HEADER, 0L);
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&file_stream));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Write::stream));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    if (progress != nullptr)
    {
      request.set(CURLOPT_XFERINFODATA, progress_data);
      request.set(CURLOPT_XFERINFOFUNCTION, progress);
      request.set(CURLOPT_NOPROGRESS, 0L);
    }

    bool is_performed = request.perform();
    this->http_code = request.status_code();

    if (callback != nullptr)
      callback(is_performed);
    return is_performed;
  }

  bool
  Client::sync_download_to(
      const std::string &remote_file,
      char *&buffer_ptr,
      unsigned long long &buffer_size,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    bool is_existed = this->check(remote_file);
    if (!is_existed)
      return false;

    auto root_urn = Path(this->webdav_root, true);
    auto file_urn = root_urn + remote_file;

    Data data = {nullptr, 0, 0};

    Request request(this->options());

    auto url = this->webdav_hostname + file_urn.quote(request.handle);
    request.set(CURLOPT_CUSTOMREQUEST, "GET");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HEADER, 0L);
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&data));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    if (progress != nullptr)
    {
      request.set(CURLOPT_XFERINFODATA, progress_data);
      request.set(CURLOPT_XFERINFOFUNCTION, progress);
      request.set(CURLOPT_NOPROGRESS, 0L);
    }

    bool is_performed = request.perform();
    this->http_code = request.status_code();

    if (callback != nullptr)
      callback(is_performed);
    if (!is_performed)
      return false;

    buffer_ptr = data.buffer;
    buffer_size = data.size;
    data.reset();
    return true;
  }

bool
  Client::sync_download_range_to(
      const std::string &remote_file,
      char *&buffer_ptr,
      unsigned long long &buffer_size,
      uint64_t range_from,
      uint64_t range_to,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    bool is_existed = this->check(remote_file);
    if (!is_existed)
      return false;

    auto root_urn = Path(this->webdav_root, true);
    auto file_urn = root_urn + remote_file;

    Data data = {nullptr, 0, 0};

    Request request(this->options());

    auto url = this->webdav_hostname + file_urn.quote(request.handle);
    struct curl_slist *list = NULL;
    char range_header[64];
    sprintf(range_header, "Range: bytes=%lu-%lu", range_from, range_to);
    list = curl_slist_append(list, range_header);
    request.set(CURLOPT_CUSTOMREQUEST, "GET");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HEADER, 0L);
    request.set(CURLOPT_HTTPHEADER, list);
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&data));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    if (progress != nullptr)
    {
      request.set(CURLOPT_XFERINFODATA, progress_data);
      request.set(CURLOPT_XFERINFOFUNCTION, progress);
      request.set(CURLOPT_NOPROGRESS, 0L);
    }

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    if (callback != nullptr)
      callback(is_performed);
    if (!is_performed)
      return false;

    buffer_ptr = data.buffer;
    buffer_size = data.size;
    data.reset();
    return true;
  }

  bool
  Client::sync_download_to(
      const std::string &remote_file,
      std::ostream &stream,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    bool is_existed = this->check(remote_file);
    if (!is_existed)
      return false;

    auto root_urn = Path(this->webdav_root, true);
    auto file_urn = root_urn + remote_file;

    Request request(this->options());

    auto url = this->webdav_hostname + file_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "GET");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HEADER, 0L);
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&stream));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Write::stream));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    if (progress != nullptr)
    {
      request.set(CURLOPT_XFERINFODATA, progress_data);
      request.set(CURLOPT_XFERINFOFUNCTION, progress);
      request.set(CURLOPT_NOPROGRESS, 0L);
    }

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    if (callback != nullptr)
      callback(is_performed);

    return is_performed;
  }

  bool
  Client::sync_upload(
      const std::string &remote_file,
      const std::string &local_file,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    bool is_existed = FileInfo::exists(local_file);
    if (!is_existed)
      return false;

    auto root_urn = Path(this->webdav_root, true);
    auto file_urn = root_urn + remote_file;

    std::ifstream file_stream(local_file, std::ios::binary);
    auto size = FileInfo::size(local_file);

    Request request(this->options());

    auto url = this->webdav_hostname + file_urn.quote(request.handle);

    Data response = {nullptr, 0, 0};

    request.set(CURLOPT_UPLOAD, 1L);
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_READDATA, reinterpret_cast<size_t>(&file_stream));
    request.set(CURLOPT_READFUNCTION, reinterpret_cast<size_t>(Callback::Read::stream));
    request.set(CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
    request.set(CURLOPT_BUFFERSIZE, static_cast<long>(Client::buffer_size));
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&response));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    if (progress != nullptr)
    {
      request.set(CURLOPT_XFERINFODATA, progress_data);
      request.set(CURLOPT_XFERINFOFUNCTION, progress);
      request.set(CURLOPT_NOPROGRESS, 0L);
    }

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    if (callback != nullptr)
      callback(is_performed);
    return is_performed;
  }

  bool
  Client::sync_upload_from(
      const std::string &remote_file,
      char *buffer_ptr,
      unsigned long long buffer_size,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    auto root_urn = Path(this->webdav_root, true);
    auto file_urn = root_urn + remote_file;

    Data data = {buffer_ptr, 0, buffer_size};

    Request request(this->options());

    auto url = this->webdav_hostname + file_urn.quote(request.handle);

    Data response = {nullptr, 0, 0};

    request.set(CURLOPT_UPLOAD, 1L);
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_READDATA, reinterpret_cast<size_t>(&data));
    request.set(CURLOPT_READFUNCTION, reinterpret_cast<size_t>(Callback::Read::buffer));
    request.set(CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(buffer_size));
    request.set(CURLOPT_BUFFERSIZE, static_cast<long>(Client::buffer_size));
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&response));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    if (progress != nullptr)
    {
      request.set(CURLOPT_XFERINFODATA, progress_data);
      request.set(CURLOPT_XFERINFOFUNCTION, progress);
      request.set(CURLOPT_NOPROGRESS, 0L);
    }

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    if (callback != nullptr)
      callback(is_performed);

    data.reset();
    return is_performed;
  }

  bool
  Client::sync_upload_from(
      const std::string &remote_file,
      std::istream &stream,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    auto root_urn = Path(this->webdav_root, true);
    auto file_urn = root_urn + remote_file;

    Request request(this->options());

    auto url = this->webdav_hostname + file_urn.quote(request.handle);
    stream.seekg(0, std::ios::end);
    size_t stream_size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    Data response = {nullptr, 0, 0};

    request.set(CURLOPT_UPLOAD, 1L);
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_READDATA, reinterpret_cast<size_t>(&stream));
    request.set(CURLOPT_READFUNCTION, reinterpret_cast<size_t>(Callback::Read::stream));
    request.set(CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(stream_size));
    request.set(CURLOPT_BUFFERSIZE, static_cast<long>(Client::buffer_size));
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&response));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    if (progress != nullptr)
    {
      request.set(CURLOPT_XFERINFODATA, progress_data);
      request.set(CURLOPT_XFERINFOFUNCTION, progress);
      request.set(CURLOPT_NOPROGRESS, 0L);
    }

    bool is_performed = request.perform();
    this->http_code = request.status_code();

    if (callback != nullptr)
      callback(is_performed);
    return is_performed;
  }

  Client::Client(const dict_t &options)
  {
    this->webdav_hostname = get(options, "webdav_hostname");
    this->webdav_root = get(options, "webdav_root");
    this->webdav_username = get(options, "webdav_username");
    this->webdav_password = get(options, "webdav_password");

    this->proxy_hostname = get(options, "proxy_hostname");
    this->proxy_username = get(options, "proxy_username");
    this->proxy_password = get(options, "proxy_password");

    this->cert_path = get(options, "cert_path");
    this->key_path = get(options, "key_path");

    auto check = get(options, "check_enabled");
    if (check.length() > 0)
      this->check_enabled = std::stoi(check);
    else
      this->check_enabled = 0;
  }

  unsigned long long
  Client::free_size()
  {
    Header header =
        {
            "Accept: */*",
            "Depth: 0",
            "Content-Type: text/xml"};

    pugi::xml_document document;
    auto propfind = document.append_child("D:propfind");
    propfind.append_attribute("xmlns:D") = "DAV:";

    auto prop = propfind.append_child("D:prop");
    prop.append_child("D:quokta-available-bytes");
    prop.append_child("D:quota-used-bytes");

    auto document_print = pugi::node_to_string(document);
    size_t size = document_print.length() * sizeof((document_print.c_str())[0]);

    Data data = {nullptr, 0, 0};

    Request request(this->options());

    request.set(CURLOPT_CUSTOMREQUEST, "PROPFIND");
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<struct curl_slist *>(header.handle));
    request.set(CURLOPT_POSTFIELDS, document_print.c_str());
    request.set(CURLOPT_POSTFIELDSIZE, static_cast<long>(size));
    request.set(CURLOPT_HEADER, 0);
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&data));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif

    auto is_performed = request.perform();
    this->http_code = request.status_code();
    if (!is_performed)
      return 0;

    document.load_buffer(data.buffer, static_cast<size_t>(data.size));

    pugi::xml_node multistatus = document.select_node("*[local-name()='multistatus']").node();
    pugi::xml_node response = multistatus.select_node("*[local-name()='response']").node();
    pugi::xml_node propstat = response.select_node("*[local-name()='propstat']").node();
    prop = propstat.select_node("*[local-name()='prop']").node();
    pugi::xml_node quota_available_bytes = prop.select_node("*[local-name()='quota-available-bytes']").node();
    std::string free_size_text = quota_available_bytes.first_child().value();

    return std::stoll(free_size_text);
  }

  bool
  Client::check(const std::string &remote_resource)
  {
    if (!this->check_enabled)
      return true;
    auto root_urn = Path(this->webdav_root, true);
    auto resource_urn = root_urn + remote_resource;

    Header header =
        {
            "Accept: */*",
            "Depth: 0"};

    Data data = {nullptr, 0, 0};

    Request request(this->options());

    auto url = this->webdav_hostname + resource_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "PROPFIND");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&data));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    return is_performed;
  }

  dict_t
  Client::info(const std::string &remote_resource)
  {
    auto root_urn = Path(this->webdav_root, true);
    auto target_urn = root_urn + remote_resource;

    Header header =
        {
            "Accept: */*",
            "Depth: 0"};

    Data data = {nullptr, 0, 0};

    Request request(this->options());

    auto url = this->webdav_hostname + target_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "PROPFIND");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&data));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    bool is_performed = request.perform();
    this->http_code = request.status_code();

    if (!is_performed)
      return dict_t{};

    pugi::xml_document document;
    document.load_buffer(data.buffer, static_cast<size_t>(data.size));

#ifdef WDC_VERBOSE
    document.save(std::cout);
#endif
    auto multistatus = document.select_node("*[local-name()='multistatus']").node();
    auto responses = multistatus.select_nodes("*[local-name()='response']");
    for (auto response : responses)
    {
      pugi::xml_node href = response.node().select_node("*[local-name()='href']").node();
      std::string encode_file_name = href.first_child().value();
      std::string resource_path = curl_unescape(encode_file_name.c_str(), static_cast<int>(encode_file_name.length()));
      auto target_path = target_urn.path();
      auto target_path_without_sep = target_urn.path();
      if (!target_path_without_sep.empty() && target_path_without_sep.back() == '/')
        target_path_without_sep.resize(target_path_without_sep.length() - 1);
      auto resource_path_without_sep = resource_path.erase(resource_path.find_last_not_of('/') + 1);
      size_t pos = resource_path_without_sep.find(this->webdav_hostname);
      if (pos != std::string::npos)
        resource_path_without_sep.erase(pos, this->webdav_hostname.length());

      if (resource_path_without_sep == target_path_without_sep)
      {
        auto propstat = response.node().select_node("*[local-name()='propstat']").node();
        auto prop = propstat.select_node("*[local-name()='prop']").node();
        auto creation_date = prop.select_node("*[local-name()='creationdate']").node();
        auto display_name = prop.select_node("*[local-name()='displayname']").node();
        auto content_length = prop.select_node("*[local-name()='getcontentlength']").node();
        auto modified_date = prop.select_node("*[local-name()='getlastmodified']").node();
        auto resource_type = prop.select_node("*[local-name()='resourcetype']").node();

        std::string name = target_urn.name();
        dict_t information =
            {
                {"created", creation_date.first_child().value()},
                {"name", Util::Rtrim(name, "/")},
                {"size", content_length.first_child().value()},
                {"modified", modified_date.first_child().value()},
                {"type", resource_type.first_child().name()}};

        return information;
      }
    }

    return dict_t{};
  }

  bool
  Client::head(const std::string &remote_resource, dict_t *headers)
  {
    auto root_urn = Path(this->webdav_root, true);
    auto target_urn = root_urn + remote_resource;

    Header header =
        {"Accept: */*"};

    Request request(this->options());

    auto url = this->webdav_hostname + target_urn.quote(request.handle);
    request.set(CURLOPT_CUSTOMREQUEST, "HEAD");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
    request.set(CURLOPT_HEADERDATA, headers);
    request.set(CURLOPT_HEADERFUNCTION, header_callback);
    request.set(CURLOPT_NOBODY, 1L);
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif
    bool is_performed = request.perform();
    this->http_code = request.status_code();

    return is_performed;
  }

  bool
  Client::is_directory(const std::string &remote_resource)
  {
    auto information = this->info(remote_resource);
    auto resource_type = information["type"];
    bool is_dir = resource_type == "d:collection" || resource_type == "D:collection";
    return is_dir;
  }

  dict_items_t
  Client::list(const std::string &remote_directory)
  {
    bool is_existed = this->check(remote_directory);
    if (!is_existed)
      return dict_items_t{};

    auto target_urn = Path(this->webdav_root, true) + remote_directory;
    target_urn = Path(target_urn.path(), true);

    Header header =
        {
            "Accept: */*",
            "Depth: 1"};

    Data data = {nullptr, 0, 0};

    Request request(this->options());

    auto url = this->webdav_hostname + target_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "PROPFIND");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
    request.set(CURLOPT_HEADER, 0);
    request.set(CURLOPT_WRITEDATA, reinterpret_cast<size_t>(&data));
    request.set(CURLOPT_WRITEFUNCTION, reinterpret_cast<size_t>(Callback::Append::buffer));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif

    bool is_performed = request.perform();
    this->http_code = request.status_code();

    if (!is_performed)
      return dict_items_t{};

    dict_items_t resources;

    pugi::xml_document document;
    document.load_buffer(data.buffer, static_cast<size_t>(data.size));
    auto multistatus = document.select_node("*[local-name()='multistatus']").node();
    auto responses = multistatus.select_nodes("*[local-name()='response']");
    for (auto response : responses)
    {
      pugi::xml_node href = response.node().select_node("*[local-name()='href']").node();
      std::string encode_file_name = href.first_child().value();
      std::string resource_path = curl_unescape(encode_file_name.c_str(), static_cast<int>(encode_file_name.length()));
      auto target_path = target_urn.path();

      auto target_path_without_sep = target_urn.path();
      if (!target_path_without_sep.empty() && target_path_without_sep.back() == '/')
        target_path_without_sep.resize(target_path_without_sep.length() - 1);
      auto resource_path_without_sep = resource_path.erase(resource_path.find_last_not_of('/') + 1);
      size_t pos = resource_path_without_sep.find(this->webdav_hostname);
      if (pos != std::string::npos)
        resource_path_without_sep.erase(pos, this->webdav_hostname.length());

      if (resource_path_without_sep == target_path_without_sep)
        continue;

      auto propstat = response.node().select_node("*[local-name()='propstat']").node();
      auto prop = propstat.select_node("*[local-name()='prop']").node();
      auto creation_date = prop.select_node("*[local-name()='creationdate']").node();
      auto display_name = prop.select_node("*[local-name()='displayname']").node();
      auto content_length = prop.select_node("*[local-name()='getcontentlength']").node();
      auto modified_date = prop.select_node("*[local-name()='getlastmodified']").node();
      auto resource_type = prop.select_node("*[local-name()='resourcetype']").node();

      Path resource_urn(resource_path);
      std::string name = resource_urn.name();
      dict_t item = {
          {"created", creation_date.first_child().value()},
          {"name", Util::Rtrim(name, "/")},
          {"size", content_length.first_child().value()},
          {"modified", modified_date.first_child().value()},
          {"type", resource_type.first_child().name()}};
      resources.push_back(item);
    }

    return resources;
  }

  bool Client::download(
      const std::string &remote_file,
      const std::string &local_file,
      progress_data_t progress_data,
      progress_t progress)
  {
    return this->sync_download(remote_file, local_file, nullptr, progress_data, std::move(progress));
  }

  void
  Client::async_download(
      const std::string &remote_file,
      const std::string &local_file,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    std::thread downloading([=]()
                            { this->sync_download(remote_file, local_file, callback, progress_data, std::move(progress)); });
    downloading.detach();
  }

  bool
  Client::download_to(
      const std::string &remote_file,
      char *&buffer_ptr,
      unsigned long long &buffer_size,
      progress_data_t progress_data,
      progress_t progress)
  {
    return this->sync_download_to(remote_file, buffer_ptr, buffer_size, nullptr, progress_data, std::move(progress));
  }

  bool
  Client::download_range_to(
      const std::string &remote_file,
      char *&buffer_ptr,
      unsigned long long &buffer_size,
      uint64_t range_from,
      uint64_t range_to,
      progress_data_t progress_data,
      progress_t progress)
  {
    return this->sync_download_range_to(remote_file, buffer_ptr, buffer_size, range_from, range_to, nullptr, progress_data, std::move(progress));
  }

  bool
  Client::download_to(
      const std::string &remote_file,
      std::ostream &stream,
      progress_data_t progress_data,
      progress_t progress)
  {
    return this->sync_download_to(remote_file, stream, nullptr, progress_data, std::move(progress));
  }

  bool
  Client::create_directory(const std::string &remote_directory, bool recursive)
  {
    bool is_existed = this->check(remote_directory);
    if (is_existed)
      return true;

    bool resource_is_dir = true;
    Path directory_urn(remote_directory, resource_is_dir);

    if (recursive)
    {
      auto remote_parent_directory = directory_urn.parent().path();
      if (remote_parent_directory == remote_directory)
        return false;
      bool is_created = this->create_directory(remote_parent_directory, true);
      if (!is_created)
        return false;
    }

    Header header =
        {
            "Accept: */*",
            "Connection: Keep-Alive"};

    auto target_urn = Path(this->webdav_root, true) + remote_directory;
    target_urn = Path(target_urn.path(), true);

    Request request(this->options());

    auto url = this->webdav_hostname + target_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "MKCOL");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    return is_performed;
  }

  bool
  Client::move(const std::string &remote_source_resource, const std::string &remote_destination_resource)
  {
    bool is_existed = this->check(remote_source_resource);
    if (!is_existed)
      return false;

    Path root_urn(this->webdav_root, true);

    auto source_resource_urn = root_urn + remote_source_resource;
    auto destination_resource_urn = root_urn + remote_destination_resource;

    Header header =
        {
            "Accept: */*",
            "Destination: " + destination_resource_urn.path()};

    Request request(this->options());

    auto url = this->webdav_hostname + source_resource_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "MOVE");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    return is_performed;
  }

  bool
  Client::copy(const std::string &remote_source_resource, const std::string &remote_destination_resource)
  {
    bool is_existed = this->check(remote_source_resource);
    if (!is_existed)
      return false;

    Path root_urn(this->webdav_root, true);

    auto source_resource_urn = root_urn + remote_source_resource;
    auto destination_resource_urn = root_urn + remote_destination_resource;

    Header header =
        {
            "Accept: */*",
            "Destination: " + destination_resource_urn.path()};

    Request request(this->options());

    auto url = this->webdav_hostname + source_resource_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "COPY");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    return is_performed;
  }

  bool
  Client::upload(
      const std::string &remote_file,
      const std::string &local_file,
      progress_data_t progress_data,
      progress_t progress)
  {
    return this->sync_upload(remote_file, local_file, nullptr, progress_data, std::move(progress));
  }

  void
  Client::async_upload(
      const std::string &remote_file,
      const std::string &local_file,
      callback_t callback,
      progress_data_t progress_data,
      progress_t progress)
  {
    std::thread uploading([=]()
                          { this->sync_upload(remote_file, local_file, callback, progress_data, std::move(progress)); });
    uploading.detach();
  }

  bool
  Client::upload_from(
      const std::string &remote_file,
      std::istream &stream,
      progress_data_t progress_data,
      progress_t progress)
  {
    return this->sync_upload_from(remote_file, stream, nullptr, progress_data, std::move(progress));
  }

  bool
  Client::upload_from(
      const std::string &remote_file,
      char *buffer_ptr,
      unsigned long long buffer_size,
      progress_data_t progress_data,
      progress_t progress)
  {
    return this->sync_upload_from(remote_file, buffer_ptr, buffer_size, nullptr, progress_data, std::move(progress));
  }

  bool
  Client::clean(const std::string &remote_resource)
  {
    bool is_existed = this->check(remote_resource);
    if (!is_existed)
      return true;

    auto root_urn = Path(this->webdav_root, true);
    auto resource_urn = root_urn + remote_resource;

    Header header =
        {
            "Accept: */*",
            "Connection: Keep-Alive"};

    Request request(this->options());

    auto url = this->webdav_hostname + resource_urn.quote(request.handle);

    request.set(CURLOPT_CUSTOMREQUEST, "DELETE");
    request.set(CURLOPT_URL, url.c_str());
    request.set(CURLOPT_HTTPHEADER, reinterpret_cast<curl_slist *>(header.handle));
#ifdef WDC_VERBOSE
    request.set(CURLOPT_VERBOSE, 1);
#endif

    bool is_performed = request.perform();
    this->http_code = request.status_code();
    return is_performed;
  }

  class Environment
  {
  public:
    Environment()
    {
      curl_global_init(CURL_GLOBAL_ALL);
    }
    ~Environment()
    {
      curl_global_cleanup();
    }
  };
} // namespace WebDAV

static const WebDAV::Environment env;
