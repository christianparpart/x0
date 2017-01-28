// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/Generator.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::hpack;

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

TEST(hpack_Generator, encodeString) {
  {
    Generator generator(4096);
    generator.encodeString("");
    ASSERT_EQ(1, generator.headerBlock().size());
    EXPECT_EQ(0x00, generator.headerBlock()[0]);
  }

  {
    Generator generator(4096);
    generator.encodeString("Hello");
    ASSERT_EQ(6, generator.headerBlock().size());
    EXPECT_EQ(0x05, generator.headerBlock()[0]);
    EXPECT_EQ("Hello", generator.headerBlock().ref(1));
  }
}

TEST(hpack_Generator, literalHeaderFieldWithIndex) {
  /* C.2.1 Literal Header Field with Indexing
   *
   * custom-key: custom-header
   *
   * 400a 6375 7374 6f6d 2d6b 6579 0d63 7573 | @.custom-key.cus
   * 746f 6d2d 6865 6164 6572                | tom-header
   */

  Generator generator(4096);

  generator.generateHeader("custom-key", "custom-header", false);
  const BufferRef& headerBlock = generator.headerBlock();

  ASSERT_EQ(26, headerBlock.size());
  EXPECT_EQ(0x40, headerBlock[0]);  // prefix
  EXPECT_EQ(0x0A, headerBlock[1]);  // first string
  EXPECT_EQ(0x0D, headerBlock[12]); // second string
}

TEST(hpack_Generator, literalHeaderWithoutIndexing) {
  /* C.2.2 Literal Header Field without Indexing
   *
   * :path: /sample/path
   *
   * 040c 2f73 616d 706c 652f 7061 7468      | ../sample/path
   */

  // we set maxSize small enough in order to avoid indexing
  Generator generator(16);
  generator.generateHeader(":path", "/sample/path", false);
  const BufferRef& headerBlock = generator.headerBlock();

  printf("hexdump:\n%s\n", headerBlock.hexdump().c_str());
  ASSERT_EQ(14, headerBlock.size());
  EXPECT_EQ(0x05, headerBlock[0]);
  EXPECT_EQ(0x0c, headerBlock[1]);
  EXPECT_EQ(0x2f, headerBlock[2]);
  EXPECT_EQ(0x73, headerBlock[3]);
}

TEST(hpack_Generator, literalHeaderNeverIndex) {
  /* C.2.3 Literal Header Field Never Indexed
   *
   * password: secret
   */

  // uint8_t block[] = {
  //     0x10, 0x08, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64,
  //     0x06, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74 };

  Generator generator(4096);
  generator.generateHeader("password", "secret", true);
  const BufferRef& headerBlock = generator.headerBlock();

  ASSERT_EQ(17, headerBlock.size());
  EXPECT_EQ(0x10, headerBlock[0]);
  EXPECT_EQ(0x08, headerBlock[1]);
  EXPECT_EQ(0x70, headerBlock[2]);
}

TEST(hpack_Generator, literalHeaderFieldFromIndex) {
  /* C.2.4 Indexed Header Field
   *
   * :method: GET
   *
   * 82
   */

  Generator generator(64);
  generator.generateHeader(":method", "GET", false);
  const BufferRef& headerBlock = generator.headerBlock();

  ASSERT_EQ(1, headerBlock.size());
  EXPECT_EQ('\x82', headerBlock[0]);
}

TEST(hpack_Generator, updateTableSize) {
  Generator generator(4096);
  generator.setMaxSize(5);
  const BufferRef& headerBlock = generator.headerBlock();

  ASSERT_EQ(1, headerBlock.size());
  EXPECT_EQ(0x25, headerBlock[0]);
}
