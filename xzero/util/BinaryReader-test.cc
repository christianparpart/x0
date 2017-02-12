// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Buffer.h>
#include <xzero/BufferUtil.h>
#include <xzero/util/BinaryReader.h>
#include <iterator>

using namespace xzero;

TEST(util_BinaryReader, parseVarUInt) {
  const uint8_t encoded[] = { 0xAC, 0x02 };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  uint64_t val = parser.parseVarUInt();
  EXPECT_EQ(300, val);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseVarSInt32) {
  const uint8_t encoded[] = { 0xd7, 0x04 };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  int32_t val = parser.parseVarSInt32();
  EXPECT_EQ(-300, val);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseVarSInt64) {
  const uint8_t encoded[] = { 0xff, 0x88, 0x0f };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  int64_t val = parser.parseVarSInt64();
  EXPECT_EQ(-123456, val);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseLengthDelimited) {
  const uint8_t encoded[] = { 4, 0, 1, 2, 3 };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  auto vec = parser.parseLengthDelimited();
  EXPECT_EQ(4, vec.size());
  EXPECT_EQ(0, vec[0]);
  EXPECT_EQ(1, vec[1]);
  EXPECT_EQ(2, vec[2]);
  EXPECT_EQ(3, vec[3]);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseFixed64) {
  const uint8_t encoded[] = { 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  uint64_t val = parser.parseFixed64();
  logf("parsed value: $0", val);
  EXPECT_EQ(0x0102030405060708llu, val);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseFixed32) {
  const uint8_t encoded[] = { 0x04, 0x03, 0x02, 0x01 };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  uint32_t val = parser.parseFixed32();
  logf("parsed value: $0", val);
  EXPECT_EQ(0x01020304lu, val);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseDouble) {
  const uint8_t encoded[] = {
    0x1f, 0x85, 0xeb, 0x51, 0xb8, 0x1e, 0x09, 0x40
  };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  double val = parser.parseDouble();
  EXPECT_EQ(3.14, val);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseFloat) {
  const uint8_t encoded[] = {
    0xc3, 0xf5, 0x48, 0x40
  };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  float val = parser.parseFloat();
  EXPECT_EQ(3.14f, val);
  EXPECT_TRUE(parser.eof());
}

TEST(util_BinaryReader, parseString) {
  const uint8_t encoded[] = {
    0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67
  };
  BinaryReader parser(std::begin(encoded), std::end(encoded));
  std::string val = parser.parseString();
  ASSERT_EQ("testing", val);
  EXPECT_TRUE(parser.eof());
}
