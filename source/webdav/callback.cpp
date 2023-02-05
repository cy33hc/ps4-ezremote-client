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

#include <algorithm>
#include <fstream>
#include <cstring>

#include "callback.hpp"

namespace WebDAV
{
  namespace Callback
  {
    namespace Read
    {
      size_t stream(char* ptr, size_t item_size, size_t item_count, void* stream)
      {
        auto in_stream = reinterpret_cast<std::istream*>(stream);
        auto read_bytes = static_cast<unsigned long long>(item_size * item_count);
        auto position = static_cast<unsigned long long>(in_stream->tellg());
        in_stream->seekg(0, std::ios::end);
        auto size = static_cast<unsigned long long>(in_stream->tellg());
        in_stream->seekg(position, std::ios::beg);
        auto rest_bytes = size - position;
        read_bytes = std::min<unsigned long long>(read_bytes, rest_bytes);
        in_stream->read(ptr, read_bytes);
        return read_bytes;
      }

      size_t buffer(char* ptr, size_t item_size, size_t item_count, void* buffer)
      {
        auto data = (Data*)buffer;
        auto size = static_cast<unsigned long long>(item_size * item_count);
        auto rest_bytes = data->size - data->position;
        auto copied_bytes = std::min<unsigned long long>(size, rest_bytes);
        memcpy(ptr, data->buffer, copied_bytes);
        data->position += copied_bytes;
        return copied_bytes;
      }
    } // namespace Read

    namespace Write
    {
      size_t stream(char* ptr, size_t item_size, size_t item_count, void* stream)
      {
        auto out_stream = reinterpret_cast<std::ostream*>(stream);
        size_t write_bytes = item_size * item_count;
        out_stream->write(ptr, write_bytes);
        return write_bytes;
      }

      size_t buffer(char* ptr, size_t item_size, size_t item_count, void* buffer)
      {
        auto data = reinterpret_cast<Data*>(buffer);
        auto size = static_cast<unsigned long long>(item_size * item_count);
        auto rest_bytes = data->size - data->position;
        auto copied_bytes = std::min<unsigned long long>(size, rest_bytes);
        memcpy(data->buffer, ptr, copied_bytes);
        data->position += copied_bytes;
        return copied_bytes;
      }
    } // namespace Write

    namespace Append
    {
      size_t buffer(char* ptr, size_t item_size, size_t item_count, void* buffer)
      {
        auto data = reinterpret_cast<Data*>(buffer);
        auto append_size = item_size * item_count;
        auto new_buffer_size = data->size + append_size;
        auto new_buffer = new char[new_buffer_size];
        if (data->size != 0) memcpy(new_buffer, data->buffer, data->size);
        memcpy(new_buffer + data->size, ptr, append_size);
        delete[] data->buffer;
        data->buffer = new_buffer;
        data->size = new_buffer_size;
        return append_size;
      }

      size_t stream(char* ptr, size_t item_size, size_t item_count, void* stream)
      {
        auto out_stream = reinterpret_cast<std::ostream*>(stream);
        size_t write_bytes = item_size * item_count;
        out_stream->seekp(0, std::ios::end);
        out_stream->write(ptr, write_bytes);
        return write_bytes;
      }
    } // namespace Append
  } // namespace Callback
} // namespace WebDAV
