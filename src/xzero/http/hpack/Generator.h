// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {

class HeaderField;
class HeaderFieldList;

namespace hpack {

class Generator {
 public:
  /**
   * Initializes the HPACK header pack generator.
   *
   * @param maxSize maximum buffer size that may be used by HPACK.
   */
  explicit Generator(size_t maxSize);

  void setMaxSize(size_t maxSize);

  /**
   * Clears the generated header block but keeps dynamic-table state.
   */
  void clear();

  /**
   * Clears out generated header block and dynamic-table state.
   */
  void reset();

  /**
   * Adds all HeaderFieldList @p fields into the header block.
   */
  void generateHeaders(const HeaderFieldList& fields);

  /**
   * Adds given HeaderField @p field to the header block.
   */
  void generateHeader(const HeaderField& field);

  /**
   * Adds given header by @p name and @p value.
   *
   * @param name Header field name.
   * @param value Header field value.
   * @param sensitive Boolean, indicating whether or not this field is
   *                  containing sensitive data. Sensitive data is not
   *                  subject of indexing on any intermediary or upstream.
   */
  void generateHeader(const std::string& name, const std::string& value,
                      bool sensitive = false);

  /**
   * Number of already generate and not yet flushed bytes.
   */
  size_t size() const noexcept;

  /**
   * Gives read-only access to the generated header block.
   */
  const BufferRef& headerBlock() const noexcept;

  /**
   * Encodes an integer.
   *
   * @param output     The output buffer to encode to; there must be at least
   *                   4 bytes available.
   * @param value      The integer value to encode.
   * @param prefixBits Number of bits for the first bytes that the encoder
   *                   is allowed to use (between 1 and 8).
   *
   * @return number of bytes used for encoding.
   */
  static size_t encodeInt(uint8_t suffix,
                          uint8_t prefixBits,
                          uint64_t value,
                          unsigned char* output);

 public: // XXX raw access, be aware !

  void encodeInt(uint8_t suffix, uint8_t prefixBits, uint64_t value);
  void encodeString(const std::string& value, bool compressed = false);
  bool isIndexable(const HeaderField& field) const;

  void encodeHeaderIndexed(size_t index,
                           bool fullMatch,
                           const std::string& name,
                           const std::string& value,
                           bool sensitive);

  void encodeHeaderLiteral(const std::string& name,
                           const std::string& value,
                           bool sensitive);
 private:
  void write8(uint8_t byte);

 private:
  DynamicTable dynamicTable_;
  Buffer headerBlock_;
};

// {{{ inlines
inline size_t Generator::size() const noexcept {
  return headerBlock_.size();
}

inline const BufferRef& Generator::headerBlock() const noexcept {
  return headerBlock_;
}

inline void Generator::write8(uint8_t byte) {
  headerBlock_.push_back((char) byte);
}
// }}}

} // namespace hpack
} // namespace http
} // namespace xzero
