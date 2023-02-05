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

#ifndef WEBDAV_CLIENT_HPP
#define WEBDAV_CLIENT_HPP

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <curl/curl.h>

namespace WebDAV
{
  using progress_data_t = void*;

  using progress_t = int(void* context,
                                       curl_off_t dltotal,
                                       curl_off_t dlnow,
                                       curl_off_t ultotal,
                                       curl_off_t ulnow);

  using callback_t = std::function<void(bool)> ;

  using strings_t = std::vector<std::string>;
  using dict_t = std::map<std::string, std::string>;
  using dict_items_t = std::vector<dict_t>;

  auto inline get(const dict_t& options, const std::string&& name) -> std::string
  {
    auto it = options.find(name);
    if (it == options.end()) return "";
    else return it->second;
  }

  ///
  /// \brief WebDAV Client
  /// \author designerror
  /// \version 1.1.4
  /// \date 3/16/2018
  ///
  class Client
  {
  public:

    ///
    /// \param[in] webdav_hostname
    /// \param[in] webdav_root
    /// \param[in] webdav_username
    /// \param[in] webdav_password
    /// \param[in] proxy_hostname
    /// \param[in] proxy_username
    /// \param[in] proxy_password
    /// \param[in] cert_path
    /// \param[in] key_path
    /// \include client/init.cpp
    ///
    explicit Client(const dict_t& options);

    ///
    /// Get free size of the WebDAV server
    /// \return size in bytes
    /// \include client/size.cpp
    ///
    auto free_size() -> unsigned long long;

    ///
    /// Check for existence of a remote resource
    /// \param[in] remote_resource
    /// \include client/check.cpp
    ///
    auto check(const std::string& remote_resource = "/") -> bool;

    ///
    /// Get information of a remote resource
    /// \param[in] remote_resource
    /// \include client/info.cpp
    ///
    auto info(const std::string& remote_resource) -> dict_t;

    ///
    /// Get information of a remote resource
    /// \param[in] remote_resource
    /// \include client/info.cpp
    ///
    auto head(const std::string& remote_resource, dict_t *headers) -> bool;

    ///
    /// Clean an remote resource
    /// \param[in] remote_resource
    /// \include client/clean.cpp
    ///
    auto clean(const std::string& remote_resource) -> bool;

    ///
    /// Checks whether the resource directory
    /// \param[in] remote_resource
    ///
    auto is_directory(const std::string& remote_resource) -> bool;

    ///
    /// List a remote directory
    /// \param[in] remote_directory
    /// \include client/list.cpp
    ///
    auto list(const std::string& remote_directory = "") -> dict_items_t;

    ///
    /// Create a remote directory
    /// \param[in] remote_directory
    /// \param[in] recursive
    /// \include client/mkdir.cpp
    ///
    auto create_directory(
      const std::string& remote_directory,
      bool recursive = false
    ) -> bool;

    ///
    /// Move a remote resource
    /// \param[in] remote_source_resource
    /// \param[in] remote_destination_resource
    /// \include client/move.cpp
    ///
    auto move(
      const std::string& remote_source_resource,
      const std::string& remote_destination_resource
    ) -> bool;

    ///
    /// Copy a remote resource
    /// \param[in] remote_source_resource
    /// \param[in] remote_destination_resource
    /// \include client/copy.cpp
    ///
    auto copy(
      const std::string& remote_source_resource,
      const std::string& remote_destination_resource
    ) -> bool;

    ///
    /// Download a remote file to a local file
    /// \param[in] remote_file
    /// \param[in] local_file
    /// \param[in] progress
    /// \snippet client/download.cpp download_to_file
    ///
    auto download(
      const std::string& remote_file,
      const std::string& local_file,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    ///
    /// Download a remote file to a buffer
    /// \param[in] remote_file
    /// \param[out] buffer_ptr
    /// \param[out] buffer_size
    /// \param[in] progress
    /// \snippet client/download.cpp download_to_buffer
    ///
    auto download_to(
      const std::string& remote_file,
      char*& buffer_ptr,
      unsigned long long& buffer_size,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    ///
    /// Download a remote file to a buffer for specified range
    /// \param[in] remote_file
    /// \param[out] buffer_ptr
    /// \param[out] buffer_size
    /// \param[in] range_from,
    /// \param[in] range_to
    /// \param[in] progress
    /// \snippet client/download.cpp download_to_buffer
    ///
    auto download_range_to(
      const std::string& remote_file,
      char*& buffer_ptr,
      unsigned long long& buffer_size,
      uint64_t range_from,
      uint64_t range_to,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    ///
    /// Download a remote file to a stream
    /// \param[in] remote_file
    /// \param[out] stream
    /// \param[in] progress
    /// \snippet client/download.cpp download_to_stream
    ///
    auto download_to(
      const std::string& remote_file,
      std::ostream& stream,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    ///
    /// Asynchronously download a remote file to a local file
    /// \param[in] remote_file
    /// \param[in] local_file
    /// \param[in] callback
    /// \param[in] progress
    /// \snippet client/download.cpp async_download_to_file
    ///
    auto async_download(
      const std::string& remote_file,
      const std::string& local_file,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> void;

    ///
    /// Upload a remote file from a local file
    /// \param[in] remote_file
    /// \param[in] local_file
    /// \param[in] progress
    /// \snippet client/upload.cpp upload_from_file
    ///
    auto upload(
      const std::string& remote_file,
      const std::string& local_file,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    ///
    /// Upload a remote file from a buffer
    /// \param[in] remote_file
    /// \param[in] buffer_ptr
    /// \param[in] buffer_size
    /// \param[in] progress
    /// \snippet client/upload.cpp upload_from_buffer
    ///
    auto upload_from(
      const std::string& remote_file,
      char* buffer_ptr,
      unsigned long long buffer_size,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    ///
    /// Upload a remote file from a stream
    /// \param[in] remote_file
    /// \param[in] stream
    /// \param[in] progress
    /// \snippet client/upload.cpp upload_from_stream
    ///
    auto upload_from(
      const std::string& remote_file,
      std::istream& stream,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    ///
    /// Asynchronously upload a remote file from a local file
    /// \param[in] remote_file
    /// \param[in] local_file
    /// \param[in] callback
    /// \param[in] progress
    /// \snippet client/upload.cpp async_upload_from_file
    ///
    auto async_upload(
      const std::string& remote_file,
      const std::string& local_file,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> void;

    long status_code();

  private:

    auto sync_download(
      const std::string& remote_file,
      const std::string& local_file,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    auto sync_download_to(
      const std::string& remote_file,
      char*& buffer_ptr,
      unsigned long long& buffer_size,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    auto sync_download_range_to(
      const std::string& remote_file,
      char*& buffer_ptr,
      unsigned long long& buffer_size,
      uint64_t range_from,
      uint64_t range_to,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    bool sync_download_to(
      const std::string& remote_file,
      std::ostream& stream,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    );

    bool sync_upload(
      const std::string& remote_file,
      const std::string& local_file,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    );

    auto sync_upload_from(
      const std::string& remote_file,
      char* buffer_ptr,
      unsigned long long buffer_size,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    auto sync_upload_from(
      const std::string& remote_file,
      std::istream& stream,
      callback_t callback = nullptr,
      progress_data_t progress_data = nullptr,
      progress_t progress = nullptr
    ) -> bool;

    enum { buffer_size = 1000 * 1000 };

    std::string webdav_hostname;
    std::string webdav_root;
    std::string webdav_username;
    std::string webdav_password;

    std::string proxy_hostname;
    std::string proxy_username;
    std::string proxy_password;

    std::string cert_path;
    std::string key_path;

    dict_t options();

    long http_code;
    int check_enabled = 0;
  };
} // namespace WebDAV

#endif
