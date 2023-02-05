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

#ifndef WEBDAV_URN_HPP
#define WEBDAV_URN_HPP

#include <cstddef>
#include <iostream>
#include <string>

namespace WebDAV
{
  namespace Urn
  {
    using std::string;
    using std::nullptr_t;

    class Path
    {
    public:

      explicit Path(const string& path_, bool force_dir = false);
      explicit Path(nullptr_t);

      auto operator+(const std::string& rhs) const -> Path;
      auto operator==(const Path& rhs) const -> bool;

      auto is_directory() const -> bool;
      auto is_root() const -> bool;
      auto name() const -> string;
      auto parent() const -> Path;
      auto path() const -> string;
      auto quote(void* request) const -> string;

    private:

      string m_path;

      static const string separate;
      static const string root;
      static const string param_separate;
      static const string query_separate;
    };
  }
}

auto operator<<(std::ostream& stream, const WebDAV::Urn::Path& path) -> std::ostream&;

#endif
