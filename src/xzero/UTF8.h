// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _STX_BASE_UTF8_H_
#define _STX_BASE_UTF8_H_

#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <locale>

namespace xzero {

class UTF8 {
public:

  static char32_t nextCodepoint(const char** cur, const char* end);

  static void encodeCodepoint(char32_t codepoint, std::string* target);

  static bool isValidUTF8(const std::string& str);
  static bool isValidUTF8(const char* str, size_t size);

};

}
#endif
