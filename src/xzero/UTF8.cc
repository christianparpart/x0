// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/UTF8.h>
#include <xzero/RuntimeError.h>

namespace xzero {

char32_t UTF8::nextCodepoint(const char** cur, const char* end_) {
  auto begin = reinterpret_cast<const uint8_t*>(*cur);
  auto end = reinterpret_cast<const uint8_t*>(end_);

  if (begin >= end) {
    return 0;
  }

  if (*begin < 0b10000000) {
    return *(*cur)++;
  }

  if ((*begin & 0b11100000) == 0b11000000) {
    if (begin + 1 >= end) {
      throw EncodingError{"invalid UTF8 encoding"};
    }

    char32_t chr;
    chr  = (*(*cur)++ & 0b00011111) << 6;
    chr |= (*(*cur)++ & 0b00111111);

    return chr;
  }

  if ((*begin & 0b11110000) == 0b11100000) {
    if (begin + 2 >= end) {
      throw EncodingError{"invalid UTF8 encoding"};
    }

    char32_t chr;
    chr  = (*(*cur)++ & 0b00001111) << 12;
    chr |= (*(*cur)++ & 0b00111111) << 6;
    chr |= (*(*cur)++ & 0b00111111);

    return chr;
  }

  if ((*begin & 0b11111000) == 0b11110000) {
    if (begin + 3 >= end) {
      throw EncodingError{"invalid UTF8 encoding"};
    }

    char32_t chr;
    chr  = (*(*cur)++ & 0b00000111) << 18;
    chr |= (*(*cur)++ & 0b00111111) << 12;
    chr |= (*(*cur)++ & 0b00111111) << 6;
    chr |= (*(*cur)++ & 0b00111111);

    return chr;
  }

  if ((*begin & 0b11111100) == 0b11111000) {
    if (begin + 4 >= end) {
      throw EncodingError{"invalid UTF8 encoding"};
    }

    char32_t chr;
    chr  = (*(*cur)++ & 0b00000011) << 14;
    chr |= (*(*cur)++ & 0b00111111) << 18;
    chr |= (*(*cur)++ & 0b00111111) << 12;
    chr |= (*(*cur)++ & 0b00111111) << 6;
    chr |= (*(*cur)++ & 0b00111111);

    return chr;
  }

  if ((*begin & 0b11111110) == 0b11111100) {
    if (begin + 5 >= end) {
      throw EncodingError{"invalid UTF8 encoding"};
    }

    char32_t chr;
    chr  = (*(*cur)++ & 0b00000001) << 30;
    chr |= (*(*cur)++ & 0b00111111) << 24;
    chr |= (*(*cur)++ & 0b00111111) << 18;
    chr |= (*(*cur)++ & 0b00111111) << 12;
    chr |= (*(*cur)++ & 0b00111111) << 6;
    chr |= (*(*cur)++ & 0b00111111);

    return chr;
  }

  throw EncodingError{"invalid UTF8 encoding"};
}

bool UTF8::isValidUTF8(const std::string& str) {
  return UTF8::isValidUTF8(str.data(), str.size());
}

bool UTF8::isValidUTF8(const char* str, size_t size) {
  auto end = str + size;

  for (auto cur = str; cur < end; ) {
    if (*reinterpret_cast<const uint8_t*>(cur) < 0b10000000) {
      cur = cur + 1;
      return true;
    }

    if ((*reinterpret_cast<const uint8_t*>(cur) & 0b11100000) == 0b11000000) {
      if (cur + 1 >= end) {
        return false;
      } else {
        cur = cur + 2;
        continue;
      }
    }

    if ((*reinterpret_cast<const uint8_t*>(cur) & 0b11110000) == 0b11100000) {
      if (cur + 2 >= end) {
        return false;
      } else {
        cur = cur + 3;
        continue;
      }
    }

    if ((*reinterpret_cast<const uint8_t*>(cur) & 0b11111000) == 0b11110000) {
      if (cur + 3 >= end) {
        return false;
      } else {
        cur = cur + 4;
        continue;
      }
    }

    if ((*reinterpret_cast<const uint8_t*>(cur) & 0b11111100) == 0b11111000) {
      if (cur + 4 >= end) {
        return false;
      } else {
        cur = cur + 5;
        continue;
      }
    }

    if ((*reinterpret_cast<const uint8_t*>(cur) & 0b11111110) == 0b11111100) {
      if (cur + 5 >= end) {
        return false;
      } else {
        cur = cur + 6;
        continue;
      }
    }
  }

  return true;
}

void UTF8::encodeCodepoint(char32_t codepoint, std::string* target) {
  if (codepoint < 0b10000000) {
    *target += (char) codepoint;
    return;
  }

  if (codepoint < 0b100000000000) {
    *target += (char) (0b11000000 | ((codepoint >> 6) & 0b00011111));
    *target += (char) (0b10000000 | (codepoint        & 0b00111111));
    return;
  }

  else if (codepoint < 0b10000000000000000) {
    *target += (char) (0b11100000 | ((codepoint >> 12) & 0b00001111));
    *target += (char) (0b10000000 | ((codepoint >> 6)  & 0b00111111));
    *target += (char) (0b10000000 | (codepoint         & 0b00111111));
    return;
  }

  else if (codepoint < 0b1000000000000000000000) {
    *target += (char) (0b11110000 | ((codepoint >> 18) & 0b00000111));
    *target += (char) (0b10000000 | ((codepoint >> 12) & 0b00111111));
    *target += (char) (0b10000000 | ((codepoint >> 6)  & 0b00111111));
    *target += (char) (0b10000000 | (codepoint         & 0b00111111));
    return;
  }

  else if (codepoint < 0b100000000000000000000000000) {
    *target += (char) (0b11111000 | ((codepoint >> 24) & 0b00000011));
    *target += (char) (0b10000000 | ((codepoint >> 18) & 0b00111111));
    *target += (char) (0b10000000 | ((codepoint >> 12) & 0b00111111));
    *target += (char) (0b10000000 | ((codepoint >> 6)  & 0b00111111));
    *target += (char) (0b10000000 | (codepoint         & 0b00111111));
  }

  else {
    *target += (char) (0b11111100 | ((codepoint >> 30) & 0b00000001));
    *target += (char) (0b10000000 | ((codepoint >> 24) & 0b00111111));
    *target += (char) (0b10000000 | ((codepoint >> 18) & 0b00111111));
    *target += (char) (0b10000000 | ((codepoint >> 11) & 0b00111111));
    *target += (char) (0b10000000 | ((codepoint >> 6)  & 0b00111111));
    *target += (char) (0b10000000 | (codepoint         & 0b00111111));
  }
}

}
