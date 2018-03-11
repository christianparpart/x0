// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/FlowLocation.h>
#include <xzero/io/FileDescriptor.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace xzero {
namespace flow {

std::string FlowLocation::str() const {
  char buf[256];
  std::size_t n =
      snprintf(buf, sizeof(buf), "{ %zu:%zu.%zu - %zu:%zu.%zu }", begin.line,
               begin.column, begin.offset, end.line, end.column, end.offset);
  return std::string(buf, n);
}

std::string FlowLocation::text() const {
  FileDescriptor fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0)
    return {};

  int size = 1 + end.offset - begin.offset;
  if (size <= 0)
    return {};

  if (lseek(fd, begin.offset, SEEK_SET) < 0)
    return {};

  std::string result;
  result.reserve(size + 1);
  ssize_t n = read(fd, const_cast<char*>(result.data()), size);
  if (n < 0)
    return {};

  result.resize(static_cast<size_t>(n));
  return result;
}

}  // namespace flow
}  // namespace xzero
