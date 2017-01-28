// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/Huffman.h>
#include <xzero/RuntimeError.h>

namespace xzero {
namespace http {
namespace hpack {

struct HuffmanCode {
  unsigned nbits;
  uint32_t code;
};

static HuffmanCode huffmanCodes[] = {
  // TODO [sym] = { nbits, code },
};

size_t Huffman::encodeLength(const std::string& value) {
  size_t n = 0;

  for (uint8_t ch: value)
    n += huffmanCodes[ch].nbits;

  return (n + 7) / 8;
}

std::string Huffman::encode(const std::string& value) {
  RAISE(NotImplementedError, "Huffman encoding not implemented yet");
}

std::string Huffman::decode(const uint8_t* pos, const uint8_t* end) {
  std::string output;
  decode(&output, pos, end);
  return output;
}

void Huffman::decode(std::string* output, const uint8_t* pos, const uint8_t* end) {
  RAISE(NotImplementedError, "Huffman decoding not implemented yet");
}

} // namespace hpack
} // namespace http
} // namespace xzero
