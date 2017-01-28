// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/Buffer.h>
#include <functional>

namespace xzero {
namespace http {
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
   * @param dynamicTable the dynamic table, used as decompression context.
   * @param maxSize initial maximum size of the internal dynamic table in bytes.
   * @param emitter callback to receive all parsed headers.
   */
  Parser(DynamicTable* context, size_t maxSize, Emitter emitter);

  /**
   * Retrieves the maximum internal maxSize limit.
   */
  size_t maxSize() const noexcept { return maxSize_; }

  /**
   * Retrieves the actual internal size limit in bytes.
   *
   * This value can never go past the set maxSize.
   */
  size_t internalMaxSize() const noexcept { return dynamicTable_->maxSize(); }

  /**
   * Parses a syntactically complete header block.
   */
  size_t parse(const_iterator pos, const_iterator end);

  size_t parse(const BufferRef& headerBlock);

 public: // helper api
  const_iterator indexedHeaderField(const_iterator pos, const_iterator end);
  const_iterator incrementalIndexedField(const_iterator pos, const_iterator end);
  const_iterator updateTableSize(const_iterator pos, const_iterator end);
  const_iterator literalHeaderNoIndex(const_iterator pos, const_iterator end);
  const_iterator literalHeaderNeverIndex(const_iterator pos, const_iterator end);

  /**
   * Retrieves an indexed header field from either static or dynamic table.
   *
   * @param index the HPACK conform index that represents the header field.
   *              A value between 1 and StaticTable::length() is a field
   *              from the static table.
   *              A value between StaticTable::length() + 1 and above
   *              will be retrieving the value from the DynamicTable's
   *              offset minus StaticTable::length().
   */
  const TableEntry& at(size_t index);

  /**
   * Decodes a variable sized unsigned integer.
   *
   * @param prefixBits number of used bits at @p pos.
   * @param output the resulting integer will be stored here.
   * @param pos beginning position to start decoding from.
   * @param end maximum position to be able to read to (excluding).
   */
  static size_t decodeInt(uint8_t prefixBits,
                          uint64_t* output,
                          const_iterator pos,
                          const_iterator end);

  /**
   * Decodes a length-encoded string.
   *
   * @param output the resulting string will be stored here.
   * @param pos beginning position to start decoding from.
   * @param end maximum position to be able to read to (excluding).
   */
  static size_t decodeString(std::string* output,
                             const_iterator pos,
                             const_iterator end);

 private:
  void emit(const std::string& name, const std::string& value);
  void emitSenstive(const std::string& name, const std::string& value, bool sensitive);

 private:
  size_t maxSize_;
  DynamicTable* dynamicTable_;
  Emitter emitter_;
};

} // namespace hpack
} // namespace http
} // namespace xzero
