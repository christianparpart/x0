// This file is part of the "x0" project
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/http/hpack.h>
#include <inttypes.h>

namespace x0 {
namespace hpack {

// {{{ StaticTable
StaticTable::StaticTable()
    : entries_({/*  0 */ {"", ""},
                /*  1 */ {":authority", ""},
                /*  2 */ {":method", "GET"},
                /*  3 */ {":method", "POST"},
                /*  4 */ {":path", "/"},
                /*  5 */ {":path", "/index.html"},
                /*  6 */ {":scheme", "http"},
                /*  7 */ {":scheme", "https"},
                /*  8 */ {":status", "200"},
                /*  9 */ {":status", "204"},

                /* 10 */ {":status", "206"},
                /* 11 */ {":status", "304"},
                /* 12 */ {":status", "400"},
                /* 13 */ {":status", "404"},
                /* 14 */ {":status", "500"},
                /* 15 */ {"accept-charset", ""},
                /* 16 */ {"accept-encoding", "gzip, deflate"},
                /* 17 */ {"accept-language", ""},
                /* 18 */ {"accept-ranges", ""},
                /* 19 */ {"accept", ""},

                /* 20 */ {"access-control-allow-origin", ""},
                /* 21 */ {"age", ""},
                /* 22 */ {"allow", ""},
                /* 23 */ {"authorization", ""},
                /* 24 */ {"cache-control", ""},
                /* 25 */ {"content-disposition", ""},
                /* 26 */ {"content-encoding", ""},
                /* 27 */ {"content-language", ""},
                /* 28 */ {"content-length", ""},
                /* 29 */ {"content-location", ""},

                /* 30 */ {"content-range", ""},
                /* 31 */ {"content-type", ""},
                /* 32 */ {"cookie", ""},
                /* 33 */ {"date", ""},
                /* 34 */ {"etag", ""},
                /* 35 */ {"expect", ""},
                /* 36 */ {"expires", ""},
                /* 37 */ {"from", ""},
                /* 38 */ {"host", ""},
                /* 39 */ {"if-match", ""},

                /* 40 */ {"if-modified-since", ""},
                /* 41 */ {"if-none-match", ""},
                /* 42 */ {"if-range", ""},
                /* 43 */ {"if-unmodified-since", ""},
                /* 44 */ {"last-modified", ""},
                /* 45 */ {"link", ""},
                /* 46 */ {"location", ""},
                /* 47 */ {"max-forwards", ""},

                /* 48 */ {"proxy-authenticate", ""},
                /* 49 */ {"proxy-authorization", ""},
                /* 50 */ {"range", ""},
                /* 51 */ {"referer", ""},
                /* 52 */ {"refresh", ""},
                /* 53 */ {"retry-after", ""},
                /* 54 */ {"server", ""},
                /* 55 */ {"set-cookie", ""},
                /* 56 */ {"strict-transport-security", ""},
                /* 57 */ {"transfer-encoding", ""},
                /* 58 */ {"user-agent", ""},
                /* 59 */ {"vary", ""},
                /* 60 */ {"via", ""},
                /* 61 */ {"www-authenticate", ""}}) {}

StaticTable::~StaticTable() {}

StaticTable* StaticTable::get() {
  static StaticTable table;
  return &table;
}
// }}}
// {{{ HeaderTable
HeaderTable::HeaderTable(size_t maxEntries)
    : maxEntries_(maxEntries), entries_() {}

void HeaderTable::resize(size_t limit) {
  maxEntries_ = limit;
  while (size() > limit) {
    entries_.pop_back();
  }
}

void HeaderTable::add(const HeaderField& field) {
  while (size() >= maxEntries_) entries_.pop_back();
  entries_.push_front(field);
}

void HeaderTable::clear() {
  // evict all
  entries_.clear();
}
// }}}
// {{{ EncoderHelper
void EncoderHelper::encodeInt(Buffer* output, uint64_t i, unsigned prefixBits) {
  assert(prefixBits >= 1 && prefixBits <= 8);

  const unsigned maxValue = (1 << prefixBits) - 1;

  if (i < maxValue) {
    output->push_back(static_cast<char>(i));
  } else {
    output->push_back(static_cast<char>(maxValue));
    i -= maxValue;

    while (i >= 128) {
      const uint8_t byte = (1 << 7) | (i & 0x7f);
      output->push_back(static_cast<char>(byte));
      i /= 128;
    }
    output->push_back(static_cast<char>(i));
  }
}

/**
 * @brief Encodes an indexed header.
 *
 * @param output   target buffer to write to
 * @param index    header index
 */
void EncoderHelper::encodeIndexed(Buffer* output, unsigned index) {
  size_t n = output->size();
  encodeInt(output, index, 7);
  output->data()[n] |= 0x80;
}

/**
 * @brief Encodes a header field with literal name and literal value.
 *
 * @param output   target buffer to write to
 * @param name     header field's name
 * @param value    header field's value
 * @param indexing whether or not given header-field should be persisted in
 *                 the header table.
 * @param huffman  whether or not to Huffman-encode
 */
void EncoderHelper::encodeLiteral(Buffer* output, const BufferRef& name,
                                  const BufferRef& value, bool indexing,
                                  bool huffman) {

  // if (huffman) {
  //   name = huffman_encode(name)
  //   value = huffman_encode(value)
  // }

  if (indexing) {
    output->push_back('\0x40');
  } else {
    output->push_back('\0x00');
  }

  encodeInt(output, name.size(), 7);
  output->push_back(name);

  encodeInt(output, value.size(), 7);
  output->push_back(value);
}

/**
 * @brief Encodes header field with indexed name and literal value.
 *
 * @param output output buffer to encode to
 * @param name name index
 * @param value value literal
 * @param huffman whether or not to huffman-encode the value literal
 */
void EncoderHelper::encodeIndexedLiteral(Buffer* output, unsigned name,
                                         const BufferRef& value, bool huffman) {
  // if (huffman) {
  //   value = huffman_encode(value);
  // }

  encodeInt(output, name, 4);
  encodeInt(output, value.size(), 7);
  output->push_back(value);
}

void EncoderHelper::encodeTableSizeChange(Buffer* output, unsigned newSize) {
  size_t n = output->size();
  encodeInt(output, newSize, 4);
  output->data()[n] |= 0x20;
}
// }}}
// {{{ DecoderHelper
uint64_t DecoderHelper::decodeInt(const BufferRef& data, unsigned prefixBits,
                                  unsigned* bytesConsumed) {

  assert(prefixBits >= 1 && prefixBits <= 8);

  const uint8_t mask = 0xFF >> (8 - prefixBits);
  unsigned index = 0;
  uint64_t result = data[index] & mask;

  if (result >= (1 << prefixBits) - 1) {
    unsigned M = 0;
    while (true) {
      index++;
      const uint8_t byte = data[index];
      result += (byte & 127) * (1 << M);
      if ((byte & 128) != 128) {
        break;
      }
      M = M + 7;
    }
  }

  if (bytesConsumed) {
    *bytesConsumed = index + 1;
  }

  return result;
}
// }}}

}  // namespace hpack
}  // namespace x0

// vim:ts=2:sw=2
