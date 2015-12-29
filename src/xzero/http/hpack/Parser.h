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

namespace hpack {

enum class ParseError {
  NoError,
  NeedMoreData,
  InternalError,
  CompressionError,
};

/**
 * Parses an HPACK header block and emits it in human readable form to a
 * header emitter.
 */
class Parser {
 public:
  typedef unsigned char value_type;
  typedef unsigned char* iterator;
  typedef const unsigned char* const_iterator;

  typedef std::function<void(const std::string& /* name */,
                             const std::string& /* value */,
                             bool /* sensitive */)>
      Emitter;

  /**
   * Initializes a new HPACK parser with given dynamic-table @p maxSize
   * and the @p emitter.
   *
   * @param maxSize initial maximum size of the internal dynamic table in bytes.
   * @param emitter callback to receive all parsed headers.
   */
  Parser(size_t maxSize, Emitter emitter);

  void setMaxSize(size_t limit);
  size_t maxSize() const noexcept { return maxSize_; }
  size_t internalMaxSize() const noexcept { return dynamicTable_.maxSize(); }

  /**
   * Parses a syntactically complete header block.
   */
  size_t parse(const_iterator pos, const_iterator end);

 public: // helper api
  const_iterator indexedHeaderField(const_iterator pos, const_iterator end);
  const_iterator incrementalIndexedField(const_iterator pos, const_iterator end);
  const_iterator updateTableSize(const_iterator pos, const_iterator end);
  const_iterator literalHeaderNoIndex(const_iterator pos, const_iterator end);
  const_iterator literalHeaderNeverIndex(const_iterator pos, const_iterator end);

  const HeaderField& at(size_t index);

  static size_t decodeInt(uint8_t prefixBits,
                          uint64_t* output,
                          const_iterator pos,
                          const_iterator end);

  static size_t decodeString(std::string* output,
                             const_iterator pos,
                             const_iterator end);

 private:
  void emit(const std::string& name, const std::string& value);
  void emitSenstive(const std::string& name, const std::string& value, bool sensitive);

 private:
  size_t maxSize_;
  DynamicTable dynamicTable_;
  Emitter emitter_;
};

} // namespace hpack
} // namespace http
} // namespace xzero
