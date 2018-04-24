// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <string>

namespace xzero {
namespace flow {

//! \addtogroup Flow
//@{

struct FilePos { // {{{
  FilePos() : line(1), column(1), offset(0) {}
  FilePos(size_t r, size_t c, size_t o) : line(r), column(c), offset(o) {}

  FilePos& set(size_t r, size_t c, size_t o) {
    line = r;
    column = c;
    offset = o;

    return *this;
  }

  void advance(char ch) {
    offset++;
    if (ch != '\n') {
      column++;
    } else {
      line++;
      column = 1;
    }
  }

  size_t line;
  size_t column;
  size_t offset;
};

inline size_t operator-(const FilePos& a, const FilePos& b) {
  if (b.offset > a.offset)
    return 1 + b.offset - a.offset;
  else
    return 1 + a.offset - b.offset;
}
// }}}

struct SourceLocation { // {{{
  SourceLocation() : filename(), begin(), end() {}
  SourceLocation(const std::string& _fileName)
      : filename(_fileName), begin(), end() {}
  SourceLocation(const std::string& _fileName, FilePos _beg, FilePos _end)
      : filename(_fileName), begin(_beg), end(_end) {}

  std::string filename;
  FilePos begin;
  FilePos end;

  SourceLocation& update(const FilePos& endPos) {
    end = endPos;
    return *this;
  }

  SourceLocation& update(const SourceLocation& endLocation) {
    end = endLocation.end;
    return *this;
  }

  std::string str() const;
  std::string text() const;
};  // }}}

inline SourceLocation operator-(const SourceLocation& end,
                                const SourceLocation& beg) {
  return SourceLocation(beg.filename, beg.begin, end.end);
}

//!@}

}  // namespace flow
}  // namespace xzero
