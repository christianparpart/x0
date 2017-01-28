// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <iterator>
#include <xzero/Buffer.h>

namespace xzero {
namespace base64 {
  inline std::string encode(const std::string& value) {
    return encode(value.begin(), value.end());
  }

  template<typename Iterator>
  inline std::string encode(Iterator begin, Iterator end) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    return encode(begin, end, alphabet);
  }

  template<typename Iterator, typename Alphabet>
  std::string encode(Iterator begin, Iterator end, Alphabet alphabet) {
    const int inputLength = std::distance(begin, end);
    const int outputLength = ((inputLength + 2) / 3 * 4) + 1;

    std::string output;
    output.resize(outputLength);

    int i = 0;
    const int e = inputLength - 2;
    const auto input = begin;
    auto out = output.begin();

    while (i < e) {
      *out++ = alphabet[(input[i] >> 2) & 0x3F];

      *out++ = alphabet[((input[i] & 0x03) << 4) |
                        ((uint8_t)(input[i + 1] & 0xF0) >> 4)];

      *out++ = alphabet[((input[i + 1] & 0x0F) << 2) |
                        ((uint8_t)(input[i + 2] & 0xC0) >> 6)];

      *out++ = alphabet[input[i + 2] & 0x3F];

      i += 3;
    }

    if (i < inputLength) {
      *out++ = alphabet[(input[i] >> 2) & 0x3F];

      if (i == (inputLength - 1)) {
        *out++ = alphabet[((input[i] & 0x03) << 4)];
        *out++ = '=';
      } else {
        *out++ = alphabet[((input[i] & 0x03) << 4) |
                          ((uint8_t)(input[i + 1] & 0xF0) >> 4)];
        *out++ = alphabet[((input[i + 1] & 0x0F) << 2)];
      }
      *out++ = '=';
    }
    size_t outlen = std::distance(output.begin(), out);
    output.resize(outlen);

    return output;
  }

  inline size_t decodeLength(const std::string& value) {
    return decodeLength(value.begin(), value.end());
  }

  template<typename Iterator>
  size_t decodeLength(Iterator begin, Iterator end) {
    return decodeLength(begin, end, indexmap);
  }

  template<typename Iterator, typename IndexTable>
  size_t decodeLength(Iterator begin, Iterator end, const IndexTable& index) {
    Iterator pos = begin;

    while (pos != end && index[static_cast<size_t>(*pos)] <= 63)
      pos++;

    int nprbytes = std::distance(begin, pos) - 1;
    int nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    return nbytesdecoded + 1;
  }

  inline std::string decode(const std::string& input) {
    std::string output;
    output.resize(decodeLength(input));
    output.resize(decode(input, &output[0]));
    return output;
  }

  inline size_t decode(const std::string& input, Buffer* output) {
    output->reserve(decodeLength(input));
    output->resize(decode(input, output->data()));
    return output->size();
  }

  template<typename Output>
  inline size_t decode(const std::string& input, Output output) {
    return decode(input.begin(), input.end(), output);
  }

  template<typename Iterator, typename Output>
  size_t decode(Iterator begin, Iterator end, Output output) {
    return decode(begin, end, indexmap, output);
  }

  template<typename Iterator, typename IndexTable, typename Output>
  size_t decode(Iterator begin, Iterator end, const IndexTable& indexmap,
                Output output) {
    auto index = [indexmap](Iterator i) -> size_t {
      return indexmap[static_cast<size_t>(*i)];
    };

    if (begin == end)
      return 0;

    // count input bytes (excluding any trailing pad bytes)
    Iterator input = begin;
    while (index(input) <= 63)
      input++;
    size_t nprbytes = std::distance(begin, input);
    size_t decodedCount = ((nprbytes + 3) / 4) * 3;

    auto out = output;
    input = begin;

    while (nprbytes > 4) {
      *out++ = index(input + 0) << 2 | index(input + 1) >> 4;
      *out++ = index(input + 1) << 4 | index(input + 2) >> 2;
      *out++ = index(input + 2) << 6 | index(input + 3);

      input += 4;
      nprbytes -= 4;
    }

    if (nprbytes > 1) {
      *(out++) = index(input + 0) << 2 | index(input + 1) >> 4;

      if (nprbytes > 2) {
        *(out++) = index(input + 1) << 4 | index(input + 2) >> 2;

        if (nprbytes > 3) {
          *(out++) = index(input + 2) << 6 | index(input + 3);
        }
      }
    }

    decodedCount -= (4 - nprbytes) & 0x03;

    return decodedCount;
  }
} // namespace base64
} // namespace xzero
