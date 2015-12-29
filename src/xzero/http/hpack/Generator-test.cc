// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/Generator.h>
#include <gtest/gtest.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::hpack;

std::string tob(uint8_t value) {
  std::string s;
  for (int i = 7; i >= 0; i--) {
    if (value & (1 << i))
      s += '1';
    else
      s += '0';
  }
  return s;
}

void dumpb(const char* msg, uint8_t value) {
  printf("%s: %s\n", msg, tob(value).c_str());
}

TEST(hpack_Generator, encodeInt) {
  unsigned char encoded[8];
  unsigned nwritten;

  /* (C.1.1) Example 1: Encoding 10 Using a 5-Bit Prefix
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | X | X | X | 0 | 1 | 0 | 1 | 0 |   10 stored on 5 bits
   *   +---+---+---+---+---+---+---+---+
   */
  nwritten = Generator::encodeInt(0, 5, 10, encoded);
  EXPECT_EQ(1, nwritten);
  EXPECT_EQ(0b00001010, encoded[0]);

  /* (C.1.2)  Example 2: Encoding 1337 Using a 5-Bit Prefix
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | X | X | X | 1 | 1 | 1 | 1 | 1 |  Prefix = 31, I = 1306
   *   | 1 | 0 | 0 | 1 | 1 | 0 | 1 | 0 |  1306>=128, encode(154), I=1306/128
   *   | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 0 |  10<128, encode(10), done
   *   +---+---+---+---+---+---+---+---+
   */
  nwritten = Generator::encodeInt(0, 5, 1337, encoded);
  EXPECT_EQ(3, nwritten);
  EXPECT_EQ(0b00011111, encoded[0]);
  EXPECT_EQ(0b10011010, encoded[1]);
  EXPECT_EQ(0b00001010, encoded[2]);

  /* (C.1.3) Example 3: Encoding 42 Starting at an Octet Boundary
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | 0 | 0 | 1 | 0 | 1 | 0 | 1 | 0 |   42 stored on 8 bits
   *   +---+---+---+---+---+---+---+---+
   */
  nwritten = Generator::encodeInt(0, 8, 42, encoded);
  EXPECT_EQ(1, nwritten);
  EXPECT_EQ(0b00101010, encoded[0]);
}
