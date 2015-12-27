// This file is part of the "x0" project
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/hpack.h>
#include <algorithm>
#include <inttypes.h>

namespace xzero {
namespace http {
namespace hpack {

// {{{ EncoderHelper
/**
 * Encodes an integer.
 *
 * @param output     The output buffer to encode to.
 * @param value      The integer value to encode.
 * @param prefixBits Number of bits for the first bytes that the encoder
 *                   is allowed to use (between 1 and 8).
 */
void EncoderHelper::encodeInt(Buffer* output, uint64_t value,
                              unsigned prefixBits) {
  assert(prefixBits >= 1 && prefixBits <= 8);

  const unsigned maxValue = (1 << prefixBits) - 1;

  if (value < maxValue) {
    output->push_back(static_cast<char>(value));
  } else {
    output->push_back(static_cast<char>(maxValue));
    value -= maxValue;

    while (value >= 128) {
      const uint8_t byte = (1 << 7) | (value & 0x7f);
      output->push_back(static_cast<char>(byte));
      value /= 128;
    }
    output->push_back(static_cast<char>(value));
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
}  // namespace http
}  // namespace xzero
