// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/Parser.h>
#include <xzero/http/hpack/StaticTable.h>
#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/http/hpack/Huffman.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace hpack {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.hpack.Parser", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

// 2^n
#define BIT(n) (1 << (n))

// LSB from 0 to n set, rest cleared
#define BITMASK(n) (BIT(n) - 1)

Parser::Parser(DynamicTable* dynamicTable, size_t maxSize, Emitter emitter)
    : maxSize_(maxSize),
      dynamicTable_(dynamicTable),
      emitter_(emitter) {
}

size_t Parser::parse(const BufferRef& headerBlock) {
  return parse((const_iterator) headerBlock.begin(),
               (const_iterator) headerBlock.end());
}

size_t Parser::parse(const_iterator beg, const_iterator end) {
  const_iterator pos = beg;

  while (pos != end) {
    value_type octet = *pos;

    if (octet & (1 << 7)) {
      // indexed header field
      pos = indexedHeaderField(pos, end);
    }
    else if (octet & (1 << 6)) {
      // literal header field with incremental indexing
      pos = incrementalIndexedField(pos, end);
    }
    else if (octet & (1 << 5)) {
      // dynamic table size update
      pos = updateTableSize(pos, end);
    }
    else if (octet & (1 << 4)) {
      // literal header field (never indexed)
      pos = literalHeaderNeverIndex(pos, end);
    }
    else {
      // literal header field (no index)
      pos = literalHeaderNoIndex(pos, end);
    }
  }

  return pos - beg;
}

Parser::const_iterator Parser::indexedHeaderField(const_iterator pos,
                                                  const_iterator end) {
  // (6.1) Indexed Header Field Representation
  uint64_t index;
  size_t n = decodeInt(7, &index, pos, end);
  pos += n;

  const TableEntry& field = at(index);
  emit(field.first, field.second);

  return pos;
}

Parser::const_iterator Parser::incrementalIndexedField(const_iterator pos,
                                                       const_iterator end) {
  // (6.2.1) Literal Header Field with Incremental Indexing

  uint64_t index;
  size_t n = decodeInt(6, &index, pos, end);
  pos += n;

  if (index != 0) {
    // (index, literal)
    const std::string& name = at(index).first;

    std::string value;
    n = decodeString(&value, pos, end);
    pos += n;

    dynamicTable_->add(name, value);
    emit(name, value);
  } else {
    // (literal, literal)
    std::string name;
    n = decodeString(&name, pos, end);
    pos += n;

    std::string value;
    n = decodeString(&value, pos, end);
    pos += n;

    dynamicTable_->add(name, value);
    emit(name, value);
  }

  return pos;
}

Parser::const_iterator Parser::updateTableSize(const_iterator pos,
                                               const_iterator end) {
  // (6.3) Dynamic Table Size Update

  uint64_t newMaxSize;
  size_t n = decodeInt(5, &newMaxSize, pos, end);
  pos += n;

  if (newMaxSize > maxSize_)
    RAISE(CompressionError, "Received a MAX_SIZE value larger than allowed.");

  dynamicTable_->setMaxSize(newMaxSize);

  return pos;
}

Parser::const_iterator Parser::literalHeaderNoIndex(const_iterator pos,
                                                    const_iterator end) {
  // (6.2.2) Literal Header Field without Indexing

  uint64_t index;
  size_t n = decodeInt(4, &index, pos, end);
  pos += n;

  if (index != 0) {
    // (index, literal)
    const std::string& name = at(index).first;

    std::string value;
    n = decodeString(&value, pos, end);
    pos += n;

    emitSenstive(name, value, false);
  } else {
    // (literal, literal)
    std::string name;
    size_t n = decodeString(&name, pos, end);
    pos += n;

    std::string value;
    n = decodeString(&value, pos, end);
    pos += n;

    emitSenstive(name, value, false);
  }

  return pos;
}

Parser::const_iterator Parser::literalHeaderNeverIndex(
    const_iterator pos,
    const_iterator end) {
  // (6.2.3) Literal Header Field Never Indexed

  uint64_t index;
  size_t n = decodeInt(4, &index, pos, end);
  pos += n;

  if (index != 0) {
    // (index, literal)
    const std::string& name = at(index).first;

    std::string value;
    n = decodeString(&value, pos, end);
    pos += n;

    emitSenstive(name, value, true);
  } else {
    // (literal, literal)
    std::string name;
    size_t n = decodeString(&name, pos, end);
    pos += n;

    std::string value;
    n = decodeString(&value, pos, end);
    pos += n;

    emitSenstive(name, value, true);
  }

  return pos;
}

const TableEntry& Parser::at(size_t index) {
  if (index == 0)
    RAISE(CompressionError, "Invalid index (must not be 0)");

  index--;

  if (index < StaticTable::length())
    return StaticTable::at(index);

  index -= StaticTable::length();

  if (index < dynamicTable_->length())
    return dynamicTable_->at(index);

  RAISE(CompressionError, "Index out of bounds");
}

size_t Parser::decodeInt(uint8_t prefixBits, uint64_t* output,
                         const_iterator pos, const_iterator end) {
  if (!(prefixBits >= 1 && prefixBits <= 8))
    RAISE(InternalError, "prefixBits must be between 0 and 8 (not %u)", prefixBits);

  if (pos == end)
    RAISE(CompressionError, "Need more data");

  *output = *pos & BITMASK(prefixBits);

  if (*output < BITMASK(prefixBits))
    return 1;

  pos++;

  size_t nbytes = 1;
  unsigned shift = 0;

  while (pos != end) {
    uint8_t octet = *pos;
    pos++;
    nbytes++;

    *output += (octet & 0b01111111) << shift;

    if ((octet & BIT(7)) == 0)
      return nbytes;

    shift += 7;
  }

  RAISE(CompressionError, "Need more data");
}

size_t Parser::decodeString(std::string* output,
                            const_iterator pos,
                            const_iterator end) {
  assert(output != nullptr);

  uint64_t slen;
  bool compressed = *pos & (1 << 7);
  size_t n = decodeInt(7, &slen, pos, end);
  pos += n;

  if (slen > static_cast<size_t>(end - pos))
    RAISE(CompressionError, "Need more data");

  if (compressed)
    Huffman::decode(output, pos, pos + slen);
  else
    output->assign((char*) pos, slen);

  n += slen;

  return n;
}

void Parser::emit(const std::string& name, const std::string& value) {
  emitSenstive(name, value, false);
}

void Parser::emitSenstive(const std::string& name, const std::string& value, bool sensitive) {
  if (emitter_) {
    emitter_(name, value, sensitive);
  }
}

} // namespace hpack
} // namespace http
} // namespace xzero
