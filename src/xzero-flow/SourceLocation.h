// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <fmt/format.h>

namespace xzero::flow {

//! \addtogroup Flow
//@{

struct FilePos { // {{{
  FilePos() : FilePos{1, 1, 0} {}
  FilePos(unsigned r, unsigned c) : FilePos{r, c, 0} {}
  FilePos(unsigned r, unsigned c, unsigned o) : line(r), column(c), offset(o) {}

  FilePos& set(unsigned r, unsigned c, unsigned o) {
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

  bool operator==(const FilePos& other) const noexcept {
    return line == other.line &&
           column == other.column &&
           offset == other.offset;
  }

  bool operator!=(const FilePos& other) const noexcept {
    return !(*this == other);
  }

  unsigned line;
  unsigned column;
  unsigned offset;
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

  bool operator==(const SourceLocation& other) const noexcept {
    return filename == other.filename &&
           begin == other.begin &&
           end == other.end;
  }

  bool operator!=(const SourceLocation& other) const noexcept {
    return !(*this == other);
  }
};  // }}}

inline SourceLocation operator-(const SourceLocation& end,
                                const SourceLocation& beg) {
  return SourceLocation(beg.filename, beg.begin, end.end);
}

//!@}

} // namespace xzero::flow

namespace fmt {
  template<>
  struct formatter<xzero::flow::FilePos> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::flow::FilePos& v, FormatContext &ctx) {
      return format_to(ctx.begin(), "{}:{}", v.line, v.column);
    }
  };
}

namespace fmt {
  template<>
  struct formatter<xzero::flow::SourceLocation> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::flow::SourceLocation& v, FormatContext &ctx) {
      if (!v.filename.empty())
        return format_to(ctx.begin(), "{}:{}", v.filename, v.begin);
      else
        return format_to(ctx.begin(), "{}", v.begin);
    }
  };
}
