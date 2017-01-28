// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Buffer.h>
#include <xzero/BufferUtil.h>
#include <xzero/protobuf/WireGenerator.h>

using namespace xzero::protobuf;
using xzero::Buffer;
using xzero::BufferUtil;

WireGenerator::ChunkWriter BufferWriter(Buffer* output) {
  return [output](const uint8_t* data, size_t len) {
    output->push_back(data, len);
  };
}

TEST(protobuf_WireGenerator, generateVarUInt_42) {
  Buffer out;
  WireGenerator(BufferWriter(&out)).generateVarUInt(42);
  ASSERT_EQ("00101010", BufferUtil::binPrint(out, true));
}

TEST(protobuf_WireGenerator, generateVarUInt_300) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateVarUInt(300);
  logf("varint(300) = $0", BufferUtil::hexPrint(&out));
  ASSERT_EQ("10101100 00000010", BufferUtil::binPrint(out, true));
}

TEST(protobuf_WireGenerator, generateSint64) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateSint64(-300);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\xd7\x04", out);
}

TEST(protobuf_WireGenerator, generateSint32) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateSint32(-300);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\xd7\x04", out);
}

TEST(protobuf_WireGenerator, generateLengthDelimited) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateLengthDelimited((const uint8_t*) "testing", 7);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x07\x74\x65\x73\x74\x69\x6e\x67", out);
}

TEST(protobuf_WireGenerator, generateFixed64) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateFixed64(3.14);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x1f\x85\xeb\x51\xb8\x1e\x09\x40", out);
}

TEST(protobuf_WireGenerator, generateFixed32) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateFixed32(3.14f);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\xc3\xf5\x48\x40", out);
}

TEST(protobuf_WireGenerator, generateString) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateString("testing");
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x07\x74\x65\x73\x74\x69\x6e\x67", out);
}

TEST(protobuf_WireGenerator, generateKey) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateKey(WireType::LengthDelimited, 2);
  ASSERT_EQ("12", BufferUtil::hexPrint(&out));
}

TEST(protobuf_WireGenerator, generateKey_String) {
  Buffer out;
  WireGenerator g(BufferWriter(&out));
  g.generateKey(WireType::LengthDelimited, 2);
  g.generateString("testing");
  ASSERT_EQ("12 07 74 65 73 74 69 6e 67", BufferUtil::hexPrint(&out));
}
