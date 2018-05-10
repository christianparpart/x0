// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/SourceLocation.h>
#include <fmt/format.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>

namespace xzero {
namespace flow {

std::string SourceLocation::str() const {
  return fmt::format("{{ {}:{}.{} - {}:{}.{} }}",
                     begin.line, begin.column, begin.offset,
                     end.line, end.column, end.offset);
}

std::string SourceLocation::text() const {
  int size = 1 + end.offset - begin.offset;
  if (size <= 0)
    return {};

  std::ifstream fs(filename);
  fs.seekg(end.offset, fs.beg);


  std::string result;
  result.reserve(size + 1);

  fs.read(const_cast<char*>(result.data()), size);
  result.resize(static_cast<size_t>(size));

  return result;
}

}  // namespace flow
}  // namespace xzero
