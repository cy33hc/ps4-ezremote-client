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

#include <curl/curl.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using std::string;
using std::vector;

#include "urn.hpp"

namespace WebDAV
{
  namespace Urn
  {

    const string Path::separate = "/";
    const string Path::root = "/";
    const string Path::param_separate = "&";
    const string Path::query_separate = "?";

    Path::Path(const string& path_, bool force_dir)
    {
      string path = path_;
      if (path_.empty()) path = Path::root;
      auto first_position = path.find(Path::separate);
      if (first_position != 0) path = Path::root + path;

      auto last_symbol_index = path.length() - 1;
      auto last_symbol = path.substr(last_symbol_index, 1);
      auto is_dir = last_symbol == Path::separate;

      if (force_dir && !is_dir) path += Path::separate;
      m_path = path;
      auto double_separte = Path::separate + Path::separate;
      bool is_find = false;
      do
      {
        auto first_position = m_path.find(double_separte);
        is_find = first_position != m_path.npos;
        if (is_find)
        {
          m_path.replace(first_position, double_separte.size(), Path::separate);
        }
      }
      while (is_find);
    }

    Path::Path(std::nullptr_t)
    {
      m_path = nullptr;
    }

    auto Path::path() const -> string
    {
      return m_path;
    }

    auto escape(void* request, const string& name) -> string
    {
      string path = curl_easy_escape(request, name.c_str(), static_cast<int>(name.length()));
      return path;
    }

    auto split(const string& text, const string& delims) -> vector<string>
    {
      vector<string> tokens;
      auto start = text.find_first_not_of(delims);
      auto end = text.npos;

      while ((end = text.find_first_of(delims, start)) != text.npos)
      {
        tokens.push_back(text.substr(start, end - start));
        start = text.find_first_not_of(delims, end);
      }
      if (start != text.npos)
      {
        tokens.push_back(text.substr(start));
      }
      return tokens;
    }

    auto Path::quote(void* request) const -> string
    {
      if (this->is_root()) return m_path;

      size_t query_pos = m_path.find_first_of(Path::query_separate);
      string query;
      string path;
      if (query_pos != string::npos)
      {
        query = m_path.substr(query_pos+1);
        path = m_path.substr(0, query_pos);
      }
      else
      {
        path = m_path;
      }
      auto names = split(path, Path::separate);
      string quote_path;

      std::for_each(names.begin(), names.end(), [&quote_path, request](string & name)
      {
        auto escape_name = escape(request, name);
        quote_path.append(Path::separate);
        quote_path.append(escape_name);
      });

      if (is_directory())
      {
        quote_path.append(Path::separate);
      }

      if (query.length()>0)
      {
        auto params = split(query, Path::param_separate);
        if (params.size() > 0)
          quote_path.append(Path::query_separate);
        std::for_each(params.begin(), params.end(), [&quote_path, request](string & param)
        {
          auto param_pair = split(param, "=");
          if (param_pair.size() == 0)
          {
            quote_path.append(Path::param_separate);
          }
          else
          {          
            quote_path.append(escape(request, param_pair[0]));
            quote_path.append("=");
            if (param_pair.size() > 1)
            {
              quote_path.append(escape(request, param_pair[1]));
            }
            quote_path.append(Path::param_separate);
          }
        });
        quote_path.pop_back();
      }
      return quote_path;
    }

    auto Path::name() const -> string
    {
      auto path = this->path();
      auto is_root = path == Path::separate;
      if (is_root) return string{""};

      if (this->is_directory())
      {
        auto path_without_slash = path.substr(0, path.length() - 1);
        auto pre_last_separate_position = path_without_slash.rfind(Path::separate);
        auto name = path.substr(pre_last_separate_position + 1);
        return name;
      }
      else
      {
        auto last_separate_position = path.rfind(Path::separate);
        auto name = path.substr(last_separate_position + 1);
        return name;
      }
    }

    auto Path::parent() const -> Path
    {
      if (this->is_root()) return Path{m_path};

      auto last_separate_position = m_path.rfind(Path::separate, m_path.length() - 2);
      if (last_separate_position == 0) return Path{Path::separate};

      auto parent = m_path.substr(0, last_separate_position + 1);
      return Path{parent};
    }

    auto Path::is_directory() const -> bool
    {
      auto path = this->path();
      auto last_symbol_index = path.length() - 1;
      auto last_symbol = path.substr(last_symbol_index, 1);
      auto is_equal = last_symbol == Path::separate;
      return is_equal;
    }

    auto Path::is_root() const -> bool
    {
      return m_path == Path::separate;
    }

    auto Path::operator+(const string& rhs) const -> Path
    {
      return Path{ m_path + rhs };
    }

    auto Path::operator==(const Path& rhs) const -> bool
    {
      if (this->is_root() && rhs.is_root()) return true;
      if (!this->is_root() && rhs.is_root()) return false;

      string lhs_path;
      bool is_dir = is_directory();
      if (is_dir)
      {
        lhs_path = m_path.substr(0, m_path.length() - 1);
      }
      else
      {
        lhs_path = m_path;
      }
      string rhs_path;
      if (rhs.is_directory())
      {
        rhs_path = rhs.path();
        rhs_path = rhs_path.substr(0, rhs_path.length() - 1);
      }
      else
      {
        rhs_path = rhs.path();
      }
      return lhs_path == rhs_path;
    }
  } // namespace Urn
} // namespace WebDAV

auto operator<<(std::ostream& stream, const WebDAV::Urn::Path& path) -> std::ostream&
{
  return stream << path.path();
}
