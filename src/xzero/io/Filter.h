// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
