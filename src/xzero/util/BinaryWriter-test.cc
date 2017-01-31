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

TEST(util_BinaryWriter, writeVarUInt_42) {
  Buffer out;
  BinaryWriter(BufferUtil::writer(&out)).writeVarUInt(42);
  ASSERT_EQ("00101010", BufferUtil::binPrint(out, true));
}

TEST(util_BinaryWriter, writeVarUInt_300) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeVarUInt(300);
  logf("varint(300) = $0", out);
  ASSERT_EQ("10101100 00000010", BufferUtil::binPrint(out, true));
}

TEST(util_BinaryWriter, writeVarSInt64) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeVarSInt64(-300);
  logf("encoded = $0", out);
  EXPECT_EQ("\xd7\x04", out);
}

TEST(util_BinaryWriter, writeVarSInt32) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeVarSInt32(-300);
  logf("encoded = $0", out);
  EXPECT_EQ("\xd7\x04", out);
}

TEST(util_BinaryWriter, writeLengthDelimited) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeLengthDelimited((const uint8_t*) "testing", 7);
  logf("encoded = $0", out);
  EXPECT_EQ("\x07\x74\x65\x73\x74\x69\x6e\x67", out);
}

TEST(util_BinaryWriter, writeFixed64) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeFixed64(0x0011223344556677llu);
  logf("encoded = $0", out);
  EXPECT_EQ("\x77\x66\x55\x44\x33\x22\x11\x00", out);
}

TEST(util_BinaryWriter, writeFixed32) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeFixed32(0x12345678);
  logf("encoded = $0", out);
  EXPECT_EQ("\x78\x56\x34\x12", out);
}

TEST(util_BinaryWriter, writeDouble) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeDouble(3.14);
  logf("encoded = $0", out);
  EXPECT_EQ("\x1f\x85\xeb\x51\xb8\x1e\x09\x40", out);
}

TEST(util_BinaryWriter, writeFloat) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeFloat(3.14f);
  logf("encoded = $0", out);
  EXPECT_EQ("\xc3\xf5\x48\x40", out);
}

TEST(util_BinaryWriter, writeString) {
  Buffer out;
  BinaryWriter g(BufferUtil::writer(&out));
  g.writeString("testing");
  logf("encoded = $0", out);
  EXPECT_EQ("\x07testing", out);
}
