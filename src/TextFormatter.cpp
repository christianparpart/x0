// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/TextFormatter.h>

namespace base {

void TextFormatter::print(Buffer* output) {}

void TextFormatter::print(char value, char /*fmt*/) { *output_ << value; }

void TextFormatter::print(int value, char /*fmt*/) { *output_ << value; }

void TextFormatter::print(long value, char /*fmt*/) { *output_ << value; }

void TextFormatter::print(long long value, char /*fmt*/) { *output_ << value; }

void TextFormatter::print(unsigned char value, char /*fmt*/) {
  *output_ << value;
}

void TextFormatter::print(unsigned int value, char /*fmt*/) {
  *output_ << value;
}

void TextFormatter::print(unsigned long value, char /*fmt*/) {
  *output_ << value;
}

void TextFormatter::print(unsigned long long value, char /*fmt*/) {
  *output_ << value;
}

void TextFormatter::print(const char* value, char /*fmt*/) {
  *output_ << value;
}

void TextFormatter::print(const Buffer& value, char /*fmt*/) {
  *output_ << value;
}

void TextFormatter::print(const BufferRef& value, char /*fmt*/) {
  *output_ << value;
}

void __f() { TextFormatter::print("Hello, %s", 42); }

}  // namespace base
