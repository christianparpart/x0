// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_AnsiColor_hpp
#define sw_x0_AnsiColor_hpp (1)

#include <xzero/Api.h>
#include <string>

namespace xzero {

class AnsiColor {
 public:
  enum Type {
    Clear = 0,
    Reset = Clear,
    Bold = 0x0001,  // 1
    Dark = 0x0002,  // 2
    Undef1 = 0x0004,
    Underline = 0x0008,  // 4
    Blink = 0x0010,      // 5
    Undef2 = 0x0020,
    Reverse = 0x0040,    // 7
    Concealed = 0x0080,  // 8
    AllFlags = 0x00FF,
    Black = 0x0100,
    Red = 0x0200,
    Green = 0x0300,
    Yellow = 0x0400,
    Blue = 0x0500,
    Magenta = 0x0600,
    Cyan = 0x0700,
    White = 0x0800,
    AnyFg = 0x0F00,
    OnBlack = 0x1000,
    OnRed = 0x2000,
    OnGreen = 0x3000,
    OnYellow = 0x4000,
    OnBlue = 0x5000,
    OnMagenta = 0x6000,
    OnCyan = 0x7000,
    OnWhite = 0x8000,
    AnyBg = 0xF000
  };

  static std::string make(Type AColor);
  static std::string colorize(Type AColor, const std::string& AText);
};

inline AnsiColor::Type operator|(AnsiColor::Type a, AnsiColor::Type b) {
  return AnsiColor::Type(int(a) | int(b));
}

}  // namespace xzero

#endif
