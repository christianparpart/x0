// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/Parser.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/Option.h>
#include <xzero/Application.h>
#include <gtest/gtest.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::hpack;

#define BIN8(i7, i6, i5, i4, i3, i2, i1, i0) \
    ((i7 << 7) | (i6 << 6) | (i5 << 5) | (i4 << 4) | \
     (i3 << 3) | (i2 << 2) | (i1 << 1) | i0)

TEST(hpack_Parser, decodeInt) {
  Application::logToStderr(LogLevel::Trace);

  static constexpr int X = 0;
  Parser parser(4096, nullptr);
  uint8_t helloInt[4] = {0};
  uint8_t* helloIntEnd = helloInt + 4;
  uint64_t decodedInt = 0;

  /* (C.1.1) Example 1: Encoding 10 Using a 5-Bit Prefix
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | X | X | X | 0 | 1 | 0 | 1 | 0 |   10 stored on 5 bits
   *   +---+---+---+---+---+---+---+---+
   */
  helloInt[0] = BIN8(X, X, X, 0, 1, 0, 1, 0);
  size_t nparsed = Parser::decodeInt(5, &decodedInt, helloInt, helloIntEnd);
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
  helloInt[0] = BIN8(X, X, X, 1, 1, 1, 1, 1);
  helloInt[1] = BIN8(1, 0, 0, 1, 1, 0, 1, 0);
  helloInt[2] = BIN8(0, 0, 0, 0, 1, 0, 1, 0);
  nparsed = Parser::decodeInt(5, &decodedInt, helloInt, helloIntEnd);
  ASSERT_EQ(3, nparsed);
  ASSERT_EQ(1337, decodedInt);

  /* (C.1.3) Example 3: Encoding 42 Starting at an Octet Boundary
   *
   *   0   1   2   3   4   5   6   7
   *   +---+---+---+---+---+---+---+---+
   *   | 0 | 0 | 1 | 0 | 1 | 0 | 1 | 0 |   42 stored on 8 bits
   *   +---+---+---+---+---+---+---+---+
   */
  helloInt[0] = BIN8(0, 0, 1, 0, 1, 0, 1, 0);
  nparsed = Parser::decodeInt(8, &decodedInt, helloInt, helloIntEnd);
  ASSERT_EQ(1, nparsed);
  ASSERT_EQ(42, decodedInt);
}

TEST(hpack_Parser, decodeString) {
  std::string decoded;
  size_t nparsed;

  const uint8_t empty[] = {0x00};
  nparsed = Parser::decodeString(&decoded, empty, std::end(empty));
  ASSERT_EQ(1, nparsed);
  ASSERT_EQ("", decoded);

  const uint8_t hello[] = { 0x05, 'H', 'e', 'l', 'l', 'o' };
  nparsed = Parser::decodeString(&decoded, hello, std::end(hello));
  ASSERT_EQ(6, nparsed);
  ASSERT_EQ("Hello", decoded);
}

TEST(hpack_Parser, literalHeaderNeverIndex) {
  /* C.2.3 Literal Header Field Never Indexed
   *
   * password: secret
   */
  uint8_t block[] = {
    0x10, 0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64,
    0x06, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74 };

  std::string name;
  std::string value;
  Option<bool> sensitive;

  auto gotcha = [&](const std::string& _name, const std::string& _value, bool s) {
    name = _name;
    value = _value;
    sensitive = Some(s);
  };

  Parser parser(4096, gotcha);
  size_t nparsed = parser.parse(block, std::end(block));

  ASSERT_EQ(17, nparsed);
  ASSERT_EQ("password", name);
  ASSERT_EQ("secret", value);
  ASSERT_TRUE(sensitive.isSome());
  ASSERT_TRUE(sensitive.get());
}

TEST(hpack_Parser, updateTableSize) {
  Parser parser(4096, nullptr);
}

