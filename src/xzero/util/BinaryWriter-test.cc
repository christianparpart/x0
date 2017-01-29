// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Buffer.h>
#include <xzero/BufferUtil.h>
#include <xzero/util/BinaryWriter.h>

using namespace xzero;

TEST(util_BinaryWriter, generateVarUInt_42) {
  Buffer out;
  BinaryWriter(BufferUtil::writer(&out)).generateVarUInt(42);
  ASSERT_EQ("00101010", BufferUtil::binPrint(out, true));
}

TEST(util_BinaryWriter, generateVarUInt_300) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateVarUInt(300);
  logf("varint(300) = $0", BufferUtil::hexPrint(&out));
  ASSERT_EQ("10101100 00000010", BufferUtil::binPrint(out, true));
}

TEST(util_BinaryWriter, generateVarSInt64) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateVarSInt64(-300);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\xd7\x04", out);
}

TEST(util_BinaryWriter, generateVarSInt32) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateVarSInt32(-300);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\xd7\x04", out);
}

TEST(util_BinaryWriter, generateLengthDelimited) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateLengthDelimited((const uint8_t*) "testing", 7);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x07\x74\x65\x73\x74\x69\x6e\x67", out);
}

TEST(util_BinaryWriter, generateFixed64) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateFixed64(0x0011223344556677llu);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x00\x11\x22\x33\x44\x55\x66\x77", out);
}

TEST(util_BinaryWriter, generateFixed32) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateFixed32(0x12345678);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x12\x34\x56\x78", out);
}

TEST(util_BinaryWriter, generateDouble) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateDouble(3.14);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x1f\x85\xeb\x51\xb8\x1e\x09\x40", out);
}

TEST(util_BinaryWriter, generateFloat) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateFloat(3.14f);
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\xc3\xf5\x48\x40", out);
}

TEST(util_BinaryWriter, generateString) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.generateString("testing");
  logf("encoded = $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x07\x74\x65\x73\x74\x69\x6e\x67", out);
}
