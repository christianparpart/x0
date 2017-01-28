// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Buffer.h>
#include <xzero/BufferUtil.h>
#include <xzero/protobuf/WireParser.h>
#include <iterator>

using namespace xzero::protobuf;
using namespace xzero;

TEST(protobuf_WireParser, parseVarUInt) {
  const uint8_t encoded[] = { 0xAC, 0x02 };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto i = parser.parseVarUInt();
  ASSERT_TRUE(i.isSuccess());
  logf("parsed value: $0", *i);
  ASSERT_EQ(300, *i);
}

TEST(protobuf_WireParser, parseSint32) {
  const uint8_t encoded[] = { 0xd7, 0x04 };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto i = parser.parseSint32();
  ASSERT_TRUE(i.isSuccess());
  logf("parsed value: $0", *i);
  ASSERT_EQ(-300, *i);
}

TEST(protobuf_WireParser, parseSint64) {
  const uint8_t encoded[] = { 0xff, 0x88, 0x0f };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto i = parser.parseSint64();
  ASSERT_TRUE(i.isSuccess());
  logf("parsed value: $0", *i);
  ASSERT_EQ(-123456, *i);
}

TEST(protobuf_WireParser, parseLengthDelimited) {
  const uint8_t encoded[] = {
    0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67
  };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto val = parser.parseLengthDelimited();
  ASSERT_TRUE(val.isSuccess());
  logf("parsed value: $0", *val);
  ASSERT_EQ("testing", *val);
}

TEST(protobuf_WireParser, parseFixed64) {
  const uint8_t encoded[] = {
    0x1f, 0x85, 0xeb, 0x51, 0xb8, 0x1e, 0x09, 0x40
  };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto val = parser.parseFixed64();
  ASSERT_TRUE(val.isSuccess());
  logf("parsed value: $0", *val);
  ASSERT_EQ(3.14, *val);
}

TEST(protobuf_WireParser, parseFixed32) {
  const uint8_t encoded[] = {
    0xc3, 0xf5, 0x48, 0x40
  };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto val = parser.parseFixed32();
  ASSERT_TRUE(val.isSuccess());
  logf("parsed value: $0", *val);
  ASSERT_EQ(3.14f, *val);
}

TEST(protobuf_WireParser, parseString) {
  const uint8_t encoded[] = {
    0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67
  };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto val = parser.parseString();
  ASSERT_TRUE(val.isSuccess());
  logf("parsed value: $0", *val);
  ASSERT_EQ("testing", *val);
}

TEST(protobuf_WireParser, parseKey) {
  const uint8_t encoded[] = { 0x1a };
  WireParser parser(std::begin(encoded), std::end(encoded));
  auto key = parser.parseKey();

  ASSERT_TRUE(key.isSuccess());

  logf("wire type = $0", key->type);
  logf("field number = $0", key->fieldNumber);

  EXPECT_EQ(WireType::LengthDelimited, key->type);
  EXPECT_EQ(3, key->fieldNumber);
}
