// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/Buffer.h>
#include <functional>

namespace xzero {
namespace http {

class HeaderField;
class HeaderFieldList;

namespace hpack {

enum class ParseError {
  NoError,
  NeedMoreData,
  InternalError,
  CompressionError,
};

class Parser {
 public:
  typedef unsigned char value_type;
  typedef unsigned char* iterator;

  typedef std::function<void(const std::string&, const std::string&)> Emitter;

  Parser(size_t maxSize, Emitter emitter);

  /**
   * Parses a syntactically complete header block.
   */
  bool parse(const BufferRef& headerBlock);

 public: // helper api
  iterator indexedHeaderField(iterator i, iterator e);
  iterator incrementalIndexedField(iterator i, iterator e);
  iterator updateTableSize(iterator i, iterator e);
  iterator literalHeader(iterator i, iterator e);

  static size_t decodeInt(uint8_t prefixBits, uint64_t* output,
                          iterator pos, iterator end);
  static size_t decodeString(std::string* output, iterator pos, iterator end);

 private:
  DynamicTable dynamicTable_;
  Emitter emitter_;
};

} // namespace hpack
} // namespace http
} // namespace xzero
