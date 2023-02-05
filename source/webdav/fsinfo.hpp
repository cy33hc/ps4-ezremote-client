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

#ifndef WEBDAV_FSINFO_HPP
#define WEBDAV_FSINFO_HPP

#include <fstream>
#include <string>

namespace WebDAV
{
  namespace FileInfo
  {
    auto exists(const std::string& path) -> bool;
    auto size(const std::string& path_file) -> unsigned long long;
  } // namespace FileInfo
} // namespace WebDAV

#endif
