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

#include "request.hpp"
#include "fsinfo.hpp"
#include "util.h"

namespace WebDAV
{
  static int sockopt_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose)
  {
    int const size = 1048576;
    if (setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == -1)
    {
      return CURL_SOCKOPT_ERROR;
    }
    if (setsockopt(curlfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == -1)
    {
      return CURL_SOCKOPT_ERROR;
    }

    return CURL_SOCKOPT_OK;
  }

  auto inline get(const dict_t &options, const std::string &&name) -> std::string
  {
    auto it = options.find(name);
    if (it == options.end())
    {
      return std::string{""};
    }
    else
    {
      return it->second;
    }
  }

  Request::Request(dict_t &&options_) : options(options_)
  {
    auto webdav_hostname = get(options, "webdav_hostname");
    auto webdav_username = get(options, "webdav_username");
    auto webdav_password = get(options, "webdav_password");

    auto proxy_hostname = get(options, "proxy_hostname");
    auto proxy_username = get(options, "proxy_username");
    auto proxy_password = get(options, "proxy_password");

    auto cert_path = get(options, "cert_path");
    auto key_path = get(options, "key_path");

    this->handle = curl_easy_init();

    this->set(CURLOPT_SSL_VERIFYHOST, 0);
    this->set(CURLOPT_SSL_VERIFYPEER, 0);

#ifdef _DEBUG
    this->set(CURLOPT_VERBOSE, 1);
#else
    this->set(CURLOPT_VERBOSE, 0);
#endif
    if (this->cert_required())
    {
      this->set(CURLOPT_SSLCERTTYPE, "PEM");
      this->set(CURLOPT_SSLKEYTYPE, "PEM");
      this->set(CURLOPT_SSLCERT, const_cast<char *>(cert_path.c_str()));
      this->set(CURLOPT_SSLKEY, const_cast<char *>(key_path.c_str()));
    }

    this->set(CURLOPT_URL, const_cast<char *>(webdav_hostname.c_str()));
    if (!webdav_username.empty())
    {
      this->set(CURLOPT_HTTPAUTH, static_cast<int>(CURLAUTH_BASIC));
      auto token = webdav_username + ":" + webdav_password;
      this->set(CURLOPT_USERPWD, const_cast<char *>(token.c_str()));
    }
    this->set(CURLOPT_SOCKOPTFUNCTION, sockopt_callback);
    this->set(CURLOPT_FOLLOWLOCATION, 1);
    this->set(CURLOPT_COOKIEJAR, "/data/ps4-webdav-client/cookies.txt");
    this->set(CURLOPT_COOKIEFILE, "/data/ps4-webdav-client/cookies.txt");

    if (!this->proxy_enabled())
      return;

    this->set(CURLOPT_PROXY, const_cast<char *>(proxy_hostname.c_str()));
    this->set(CURLOPT_PROXYAUTH, static_cast<int>(CURLAUTH_BASIC));

    if (proxy_username.empty())
      return;

    if (proxy_password.empty())
    {
      this->set(CURLOPT_PROXYUSERNAME, const_cast<char *>(proxy_username.c_str()));
    }
    else
    {
      if (!webdav_username.empty() && !webdav_password.empty())
      {
        auto token = proxy_username + ":" + proxy_password;
        this->set(CURLOPT_PROXYUSERPWD, const_cast<char *>(token.c_str()));
      }
    }
  }

  Request::~Request() noexcept
  {
    if (this->handle != nullptr)
      curl_easy_cleanup(this->handle);
  }

  auto Request::swap(Request &other) noexcept -> void
  {
    using std::swap;
    swap(handle, other.handle);
  }

  Request::Request(Request &&other) noexcept : handle{
                                                   other.handle}
  {
    other.handle = nullptr;
  }

  auto Request::operator=(Request &&other) noexcept -> Request &
  {
    if (this != &other)
    {
      Request(std::move(other)).swap(*this);
    }
    return *this;
  }

  bool Request::perform() noexcept
  {
    if (this->handle == nullptr)
      return false;
    auto is_performed = check_code(curl_easy_perform(this->handle));
    if (!is_performed)
      return false;
    this->http_code = 0;
    curl_easy_getinfo(this->handle, CURLINFO_RESPONSE_CODE, &this->http_code);
    if (this->http_code < 200 || this->http_code > 299)
      return false;
    return true;
  }

  bool Request::proxy_enabled() const noexcept
  {
    auto proxy_hostname = get(options, "proxy_hostname");
    auto proxy_username = get(options, "proxy_username");
    auto proxy_password = get(options, "proxy_password");
    bool proxy_hostname_presented = !proxy_hostname.empty();
    if (!proxy_hostname_presented)
      return false;
    bool proxy_username_presented = !proxy_username.empty();
    bool proxy_password_presented = !proxy_password.empty();
    if (proxy_password_presented && !proxy_username_presented)
      return false;
    return true;
  }

  bool Request::cert_required() const noexcept
  {
    const auto cert_path = get(options, "cert_path");
    const auto key_path = get(options, "key_path");
    if (cert_path.empty())
      return false;
    bool cert_is_existed = FileInfo::exists(cert_path);
    if (!cert_is_existed)
      return false;
    if (key_path.empty())
      return false;
    return FileInfo::exists(key_path);
  }

  long Request::status_code() const noexcept
  {
    return this->http_code;
  }

} // namespace WebDAV
