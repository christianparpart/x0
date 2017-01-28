// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/Parser.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/Option.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::hpack;

TEST(hpack_Parser, decodeInt) {
  DynamicTable dt(4096);
  Parser parser(&dt, 4096, nullptr);
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
  helloInt[0] = 0b00001010;
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
  helloInt[0] = 0b00011111;
  helloInt[1] = 0b10011010;
  helloInt[2] = 0b00001010;
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
  helloInt[0] = 0b00101010;
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

TEST(hpack_Parser, literalHeaderFieldWithIndex) {
  /* C.2.1 Literal Header Field with Indexing
   *
   * custom-key: custom-header
   *
   * 400a 6375 7374 6f6d 2d6b 6579 0d63 7573 | @.custom-key.cus
   * 746f 6d2d 6865 6164 6572                | tom-header
   */

  uint8_t block[] = {
      0x40, 0x0a, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x6b, 0x65,
      0x79, 0x0d, 0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, 0x2d, 0x68, 0x65,
      0x61, 0x64, 0x65, 0x72 };

  std::string name;
  std::string value;
  Option<bool> sensitive;

  auto gotcha = [&](const std::string& _name, const std::string& _value, bool s) {
    name = _name;
    value = _value;
    sensitive = Some(s);
  };

  DynamicTable dt(4096);
  Parser parser(&dt, 4096, gotcha);
  size_t nparsed = parser.parse(block, std::end(block));

  ASSERT_EQ(26, nparsed);
  ASSERT_EQ("custom-key", name);
  ASSERT_EQ("custom-header", value);
  ASSERT_TRUE(sensitive.isSome());
  ASSERT_FALSE(sensitive.get());
}

TEST(hpack_Parser, literalHeaderWithoutIndexing) {
  /* C.2.2 Literal Header Field without Indexing
   *
   * :path: /sample/path
   *
   * 040c 2f73 616d 706c 652f 7061 7468      | ../sample/path
   */

  uint8_t block[] = {
      0x04, 0x0c, 0x2f, 0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2f,
      0x70, 0x61, 0x74, 0x68 };

  std::string name;
  std::string value;
  Option<bool> sensitive;

  auto gotcha = [&](const std::string& _name, const std::string& _value, bool s) {
    name = _name;
    value = _value;
    sensitive = Some(s);
  };

  DynamicTable dt(4096);
  Parser parser(&dt, 4096, gotcha);
  size_t nparsed = parser.parse(block, std::end(block));

  ASSERT_EQ(14, nparsed);
  ASSERT_EQ(":path", name);
  ASSERT_EQ("/sample/path", value);
  ASSERT_TRUE(sensitive.isSome());
  ASSERT_FALSE(sensitive.get());
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

  DynamicTable dt(4096);
  Parser parser(&dt, 4096, gotcha);
  size_t nparsed = parser.parse(block, std::end(block));

  ASSERT_EQ(17, nparsed);
  ASSERT_EQ("password", name);
  ASSERT_EQ("secret", value);
  ASSERT_TRUE(sensitive.isSome());
  ASSERT_TRUE(sensitive.get());
}

TEST(hpack_Parser, literalHeaderFieldFromIndex) {
  /* C.2.4 Indexed Header Field
   *
   * :method: GET
   *
   * 82
   */

  uint8_t block[] = { 0x82 };

  std::string name;
  std::string value;
  Option<bool> sensitive;

  auto gotcha = [&](const std::string& _name, const std::string& _value, bool s) {
    name = _name;
    value = _value;
    sensitive = Some(s);
  };

  DynamicTable dt(4096);
  Parser parser(&dt, 4096, gotcha);
  size_t nparsed = parser.parse(block, std::end(block));

  ASSERT_EQ(1, nparsed);
  ASSERT_EQ(":method", name);
  ASSERT_EQ("GET", value);
  ASSERT_TRUE(sensitive.isSome());
  ASSERT_FALSE(sensitive.get());
}

TEST(hpack_Parser, updateTableSize) {
  uint8_t block[] = { 0x25 };

  DynamicTable dt(4096);
  Parser parser(&dt, 4096, nullptr);
  size_t nparsed = parser.parse(block, std::end(block));

  ASSERT_EQ(1, nparsed);
  ASSERT_EQ(5, parser.internalMaxSize());
}

// TODO: C.3.x three requests without huffman coding
// TODO: C.4.x three requests with huffman coding
