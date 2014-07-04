// This file is part of the "x0" project
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/Api.h>
#include <x0/Buffer.h>
#include <map>
#include <list>
#include <deque>
#include <vector>
#include <utility>
#include <string>
#include <assert.h>
#include <stdint.h>

namespace x0 {
namespace hpack {

typedef std::string HeaderFieldName;
typedef std::string HeaderFieldValue;
typedef std::pair<HeaderFieldName, HeaderFieldValue> HeaderField;
typedef std::list<HeaderField> HeaderSet;

class X0_API StaticTable {
 public:
  StaticTable();
  ~StaticTable();

  static StaticTable* get();

 private:
  std::vector<HeaderField> entries_;
};

class X0_API HeaderTable {
 public:
  explicit HeaderTable(size_t maxEntries);
  ~HeaderTable();

  bool empty() const { return entries_.empty(); }
  size_t size() const { return entries_.size(); }

  void resize(size_t limit);
  void add(const HeaderField& field);
  void clear();

  const HeaderField& entry(unsigned index) const {
    assert(index >= 1 && index <= size());
    return entries_[index - 1];
  }

  const HeaderField& first() const {
    assert(!empty());
    return entry(1);
  }

  const HeaderField& last() const {
    assert(!empty());
    return entry(size());
  }

 private:
  size_t maxEntries_;
  std::deque<HeaderField> entries_;
};

// used for differential encoding of a new header set
class X0_API ReferenceSet {
 public:
  explicit ReferenceSet(HeaderTable* target);
  ~ReferenceSet();

 private:
  HeaderTable* target_;
  std::map<HeaderFieldName, HeaderFieldValue> references_;
};

class X0_API EncoderHelper {
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

class X0_API Encoder : private EncoderHelper {
 public:
  Encoder();
  ~Encoder();

  void encode(const HeaderSet& headerBlock);
};

class X0_API DecoderHelper {
 public:
  static uint64_t decodeInt(const BufferRef& input, unsigned prefixBits,
                            unsigned* bytesConsumed);
};

class X0_API Decoder : private DecoderHelper {
 public:
  Decoder();
  ~Decoder();

  void decode(const BufferRef& headerBlock);
};

}  // namespace hpack
}  // namespace x0

// vim:ts=2:sw=2
