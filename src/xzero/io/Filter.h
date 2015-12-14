// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <list>
#include <memory>

namespace xzero {

class Buffer;
class BufferRef;
class FileView;

/**
 * I/O Filter API.
 */
class XZERO_BASE_API Filter {
 public:
  /**
   * Applies this filter to the given @p input.
   *
   * @param input the input data this filter to apply to.
   * @param output the output to store the filtered data to.
   * @param last indicator whether or not this is the last data chunk in the
   *             stream.
   */
  virtual void filter(const BufferRef& input, Buffer* output, bool last) = 0;

  static void applyFilters(
    const std::list<std::shared_ptr<Filter>>& filters,
    const BufferRef& input, Buffer* output, bool last);

  static void applyFilters(
    const std::list<std::shared_ptr<Filter>>& filters,
    const FileView& input, Buffer* output, bool last);
};

} // namespace xzero
