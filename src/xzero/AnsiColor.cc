// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/AnsiColor.h>
#include <xzero/Buffer.h>

namespace xzero {

/**
 * \class AnsiColor
 * \brief provides an API to ANSI colouring.
 */

/**
 * constructs the ANSII color indicator.
 * \param AColor a bitmask of colors/flags to create the ANSII sequence for
 * \return the ANSII sequence representing the colors/flags passed.
 */
std::string AnsiColor::make(Type AColor) {
  Buffer sb;
  int i = 0;

  sb.push_back("\x1B[");  // XXX: '\e' = 0x1B

  if (AColor) {
    // special flags
    if (AColor & AllFlags) {
      for (int k = 0; k < 8; ++k) {
        if (AColor & (1 << k)) { // FIXME
          if (i++) {
            sb.push_back(';');
          }
          sb.push_back(k + 1);
        }
      }
    }

    // forground color
    if (AColor & AnyFg) {
      if (i++) {
        sb.push_back(';');
      }
      sb.push_back(((AColor >> 8) & 0x0F) + 29);
    }

    // background color
    if (AColor & AnyBg) {
      if (i++) {
        sb.push_back(';');
      }
      sb.push_back(((AColor >> 12) & 0x0F) + 39);
    }
  } else {
    sb.push_back('0');  // Clear
  }

  sb.push_back("m");

  return sb.str();
}

/**
 * constructs a coloured string.
 * \param AColor the colors/flags bitmask to colorize the given text in.
 * \param AText the text to be colourized.
 * \return the given text colourized in the expected colours/flags.
 */
std::string AnsiColor::colorize(Type AColor, const std::string& AText) {
  return make(AColor) + AText + make(Clear);
}

}
