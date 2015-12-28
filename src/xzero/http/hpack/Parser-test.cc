// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/Parser.h>
#include <xzero/http/HeaderFieldList.h>
#include <gtest/gtest.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::hpack;

#define BIN8(i7, i6, i5, i4, i3, i2, i1, i0) \
    ((i7 << 7) | (i6 << 6) | (i5 << 5) | (i4 << 4) | \
     (i3 << 3) | (i2 << 2) | (i1 << 1) | i0)

TEST(http_hpack_Parser, updateTableSize) {
  Parser parser(4096, nullptr);
}

// ----------------------------------------------------------------------
TEST(http_hpack_Parser, decodeInt) {
  static constexpr int X = 0;
  Parser parser(4096, nullptr);
  uint8_t encodedInt[4] = {0};
  uint8_t* encodedIntEnd = encodedInt + 4;
  uint64_t decodedInt = 0;

  /* (C.1.1) Example 1: Encoding 10 Using a 5-Bit Prefix
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | X | X | X | 0 | 1 | 0 | 1 | 0 |   10 stored on 5 bits
   *   +---+---+---+---+---+---+---+---+
   */
  encodedInt[0] = BIN8(X, X, X, 0, 1, 0, 1, 0);
  size_t nparsed = Parser::decodeInt(5, &decodedInt, encodedInt, encodedIntEnd);
  ASSERT_EQ(1, nparsed);
  ASSERT_EQ(10, decodedInt);

  /* (C.1.2)  Example 2: Encoding 1337 Using a 5-Bit Prefix
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | X | X | X | 1 | 1 | 1 | 1 | 1 |  Prefix = 31, I = 1306
   *   | 1 | 0 | 0 | 1 | 1 | 0 | 1 | 0 |  1306>=128, encode(154), I=1306/128
   *   | 0 | 0 | 0 | 0 | 1 | 0 | 1 | 0 |  10<128, encode(10), done
   *   +---+---+---+---+---+---+---+---+
   */
  encodedInt[0] = BIN8(X, X, X, 1, 1, 1, 1, 1);
  encodedInt[1] = BIN8(1, 0, 0, 1, 1, 0, 1, 0);
  encodedInt[2] = BIN8(0, 0, 0, 0, 1, 0, 1, 0);
  nparsed = Parser::decodeInt(5, &decodedInt, encodedInt, encodedIntEnd);
  ASSERT_EQ(3, nparsed);
  ASSERT_EQ(1337, decodedInt);

  /* (C.1.3) Example 3: Encoding 42 Starting at an Octet Boundary
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | 0 | 0 | 1 | 0 | 1 | 0 | 1 | 0 |   42 stored on 8 bits
   *   +---+---+---+---+---+---+---+---+
   */
  encodedInt[0] = BIN8(0, 0, 1, 0, 1, 0, 1, 0);
  nparsed = Parser::decodeInt(8, &decodedInt, encodedInt, encodedIntEnd);
  ASSERT_EQ(1, nparsed);
  ASSERT_EQ(42, decodedInt);
}

TEST(http_hpack_Parser, decodeString) {
  // TODO
}

TEST(http_hpack_Parser, literalHeader) {
  // TODO
}


