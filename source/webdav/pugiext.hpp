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

#ifndef WEBDAV_PUGIEXT_HPP
#define WEBDAV_PUGIEXT_HPP

#include <pugixml.hpp>

namespace pugi
{
  class PUGIXML_CLASS xml_string_writer: public xml_writer
  {
  public:
    std::string result;

    void write(const void* data, size_t size) final
    {
      result += std::string(static_cast<const char*>(data), size);
    }
  };

  inline std::string node_to_string(const pugi::xml_node& node)
  {
    xml_string_writer writer;
    node.print(writer);

    return writer.result;
  }
} // namespace pugi

#endif
