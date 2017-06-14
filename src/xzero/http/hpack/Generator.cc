// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/Generator.h>
#include <xzero/http/hpack/StaticTable.h>
#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/http/hpack/Huffman.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HeaderField.h>
#include <xzero/StringUtil.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>

extern std::string tob(uint8_t value);

namespace xzero {
namespace http {
namespace hpack {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.hpack.Generator", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

//! 2^n, mask given bit, rest cleared
#define BIT(n) (1 << (n))

//! least significant bits (from 0) to n set, rest cleared
#define maskLSB(n) ((1 << (n)) - 1)

Generator::Generator(size_t maxSize)
    : dynamicTable_(maxSize),
      headerBlock_() {
  headerBlock_.reserve(maxSize);
}

void Generator::setMaxSize(size_t maxSize) {
  headerBlock_.reserve(maxSize);
  dynamicTable_.setMaxSize(maxSize);
  encodeInt(1, 5, maxSize);
}

void Generator::clear() {
  headerBlock_.clear();
}

void Generator::reset() {
  dynamicTable_.clear();
  headerBlock_.clear();

  // set header table size to 0 (for full eviction)
  encodeInt(1, 5, (size_t) 0);

  // now set it back to some meaningful value
  encodeInt(1, 5, dynamicTable_.maxSize());
}

void Generator::generateHeaders(const HeaderFieldList& fields) {
  for (const HeaderField& field: fields) {
    generateHeader(field);
  }
}

void Generator::generateHeader(const HeaderField& field) {
  generateHeader(field.name(), field.value(), field.isSensitive());
}

void Generator::generateHeader(const std::string& name,
                               const std::string& value,
                               bool sensitive) {
  bool nameValueMatch;
  size_t index;

  std::string lwrName = name;
  StringUtil::toLower(&lwrName);

  if (StaticTable::find(lwrName, value, &index, &nameValueMatch)) {
    printf("found header in static table\n");
    encodeHeaderIndexed(index + 1, nameValueMatch, lwrName, value, sensitive);
  } else if (dynamicTable_.find(lwrName, value, &index, &nameValueMatch)) {
    printf("found header in dynamic table\n");
    encodeHeaderIndexed(index + StaticTable::length(),
                        nameValueMatch, lwrName, value, sensitive);
  } else {
    printf("found header nowhere\n");
    encodeHeaderLiteral(lwrName, value, sensitive);
  }
}

void Generator::encodeHeaderIndexed(size_t index,
                                    bool nameValueMatch,
                                    const std::string& name,
                                    const std::string& value,
                                    bool sensitive) {
  const size_t fieldSize = name.size() + value.size() +
                           DynamicTable::HeaderFieldOverheadSize;

  if (nameValueMatch) {
    // (6.1) indexed header field
    encodeInt(1, 7, index);
  } else if (sensitive) {
    // (6.2.3) indexed name, literal value, never index
    encodeInt(1, 4, index);
    encodeString(value);
  } else if (fieldSize < dynamicTable_.maxSize()) {
    // (6.2.1) indexed name, literal value, indexable
    dynamicTable_.add(name, value);
    encodeInt(1, 6, index);
    encodeString(value);
  } else {
    // (6.2.2) indexed name, literal value, non-indexable
    encodeInt(0, 4, index);
    encodeString(value);
  }
}

void Generator::encodeHeaderLiteral(const std::string& name,
                                    const std::string& value,
                                    bool sensitive) {
  const size_t fieldSize = name.size() + value.size() +
                           DynamicTable::HeaderFieldOverheadSize;

  if (sensitive) {
    // (6.2.3) Literal Header Field Never Indexed (new name)
    write8(1 << 4);
    encodeString(name);
    encodeString(value);
  } else if (fieldSize < dynamicTable_.maxSize()) {
    // (6.2.1) Literal Header Field with Incremental Indexing (new name)
    dynamicTable_.add(name, value);
    write8(1 << 6);
    encodeString(name);
    encodeString(value);
  } else {
    // (6.2.2) Literal Header Field without Indexing (new name)
    write8(0);
    encodeString(name);
    encodeString(value);
  }
}

void Generator::encodeInt(uint8_t suffix, uint8_t prefixBits, uint64_t value) {
  headerBlock_.reserve(headerBlock_.size() + 8);
  unsigned char* output = (unsigned char*) headerBlock_.end();
  size_t n = encodeInt(suffix, prefixBits, value, output);
  headerBlock_.resize(headerBlock_.size() + n);
}

void Generator::encodeString(const std::string& value, bool compressed) {
  // (5.2) String Literal Representation

  if (compressed && Huffman::encodeLength(value) < value.size()) {
    std::string smaller = Huffman::encode(value);
    encodeInt(1, 7, smaller.size());
    headerBlock_.push_back(smaller);
  } else {
    // Huffman encoding disabled
    encodeInt(0, 7, value.size());
    headerBlock_.push_back(value);
  }
}

size_t Generator::encodeInt(uint8_t suffix,
                            uint8_t prefixBits,
                            uint64_t value,
                            unsigned char* output) {
  assert(prefixBits >= 1 && prefixBits <= 8);

  const unsigned maxValue = maskLSB(prefixBits);

  if (value < maxValue) {
    *output = (suffix << prefixBits) | static_cast<uint8_t>(value);
    return 1;
  }

  *output++ = maxValue;
  value -= maxValue;

  size_t n = 2;

  while (value > maskLSB(7)) {
    const unsigned char byte = BIT(7) | (value & maskLSB(7));
    *output++ = byte;
    value >>= 7;
    n++;
  }

  *output = static_cast<unsigned char>(value);
  return n;
}

} // namespace hpack
} // namespace http
} // namespace xzero
