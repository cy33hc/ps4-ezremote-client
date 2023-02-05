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

#ifndef WEBDAV_REQUEST_HPP
#define WEBDAV_REQUEST_HPP

#include <curl/curl.h>
#include <map>
#include <string>

namespace WebDAV
{
  bool inline check_code(CURLcode code)
  {
    return code == CURLE_OK;
  }

  using dict_t = std::map<std::string, std::string>;

  class Request
  {
  public:
    explicit Request(dict_t &&options_);
    Request(const Request &other) = delete;
    Request(Request &&other) noexcept;
    ~Request() noexcept;

    auto operator=(const Request &other) -> Request & = delete;
    auto operator=(Request &&other) noexcept -> Request &;

    template <typename T>
    auto set(CURLoption option, T value) const noexcept -> bool
    {
      if (this->handle == nullptr)
        return false;
      return check_code(curl_easy_setopt(this->handle, option, value));
    }

    bool perform() noexcept;
    long status_code() const noexcept;
    void *handle;

  private:
    const dict_t options;
    dict_t response_header = {};
    long http_code;
    bool proxy_enabled() const noexcept;
    bool cert_required() const noexcept;
    auto swap(Request &other) noexcept -> void;
  };
} // namespace WebDAV

#endif
