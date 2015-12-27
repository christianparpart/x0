// This file is part of the "x0" project
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/http/HeaderField.h>
#include <xzero/Buffer.h>

#include <map>
#include <list>
#include <deque>
#include <vector>
#include <utility>
#include <string>
#include <assert.h>
#include <stdint.h>

namespace xzero {
namespace http {

class HeaderFieldList;

namespace hpack {

/**
 * List of headers.
 *
 * The header table (see Section 3.2) is a component used
 * to associate stored header fields to index values.
 */
class HeaderTable {
 public:
  explicit HeaderTable(size_t maxEntries);
  ~HeaderTable();

  size_t maxEntries() const { return maxEntries_; }
  void setMaxEntries(size_t value);

  bool empty() const { return entries_.empty(); }
  size_t size() const { return entries_.size(); }

  void add(const HeaderField& field);
  bool contains(const HeaderFieldName& name) const;
  bool contains(const HeaderField* name) const;
  void remove(const HeaderFieldName& name);
  void remove(const HeaderField* field);
  void clear();

  const HeaderField* entry(unsigned index) const {
    assert(index >= 1 && index <= size());
    return &entries_[index - 1];
  }

  const HeaderField& first() const {
    assert(!empty());
    return *entry(1);
  }

  const HeaderField& last() const {
    assert(!empty());
    return *entry(size());
  }

  typedef std::deque<HeaderField>::iterator iterator;
  typedef std::deque<HeaderField>::const_iterator const_iterator;

  iterator begin() { return entries_.begin(); }
  iterator end() { return entries_.end(); }

  const_iterator cbegin() const { return entries_.cbegin(); }
  const_iterator cend() const { return entries_.cend(); }

  iterator find(const HeaderFieldName& name);
  const_iterator find(const HeaderFieldName& name) const;

 private:
  size_t maxEntries_;
  std::deque<HeaderField> entries_;
};

/**
 * Helper methods for encoding header fragments.
 */
class EncoderHelper {
 public:
  static void encodeInt(Buffer* output, uint64_t i, unsigned prefixBits);
  static void encodeIndexed(Buffer* output, unsigned index);
  static void encodeLiteral(Buffer* output, const BufferRef& name,
                            const BufferRef& value, bool indexing,
                            bool huffman);
  static void encodeIndexedLiteral(Buffer* output, unsigned index,
                                   const BufferRef& value, bool huffman);
  static void encodeTableSizeChange(Buffer* output, unsigned newSize);
};

class Encoder : private EncoderHelper {
 public:
  Encoder();
  ~Encoder();

  void encode(const HeaderFieldList& headerBlock);
};

/**
 * Helper methods for decoding header fragments.
 */
class DecoderHelper {
 public:
  static uint64_t decodeInt(const BufferRef& input, unsigned prefixBits,
                            unsigned* bytesConsumed);
};

class Decoder : private DecoderHelper {
 public:
  Decoder();
  ~Decoder();

  void decode(const BufferRef& headerBlock);
};

} // namespace hpack
} // namespace http
} // namespace xzero
