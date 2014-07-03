// This file is part of the "x0" project
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT


#include <gtest/gtest.h>
#include <x0/http/hpack.h>

using namespace x0;
using namespace x0::hpack;

TEST(hpack_Encoder, encodeInt_0x00) {
  Buffer buf;
  EncoderHelper::encodeInt(&buf, 0x00, 8);

  ASSERT_EQ(1, buf.size());
  ASSERT_EQ('\x00', buf[0]);

  // decode
  unsigned nb = 0;
  const uint64_t decoded = DecoderHelper::decodeInt(buf, 8, &nb);

  ASSERT_EQ(1, nb);
  ASSERT_EQ(0x00, decoded);
}

TEST(hpack_Encoder, encodeInt_0xFFFFFF) {
  Buffer buf;
  EncoderHelper::encodeInt(&buf, 0xFFFFFF, 8);

  ASSERT_EQ(5, buf.size());
  ASSERT_EQ('\xFF', buf[0]);
  ASSERT_EQ('\x80', buf[1]);
  ASSERT_EQ('\xFE', buf[2]);
  ASSERT_EQ('\xFF', buf[3]);
  ASSERT_EQ('\x07', buf[4]);

  // decode
  unsigned nb = 0;
  const uint64_t decoded = DecoderHelper::decodeInt(buf, 8, &nb);

  ASSERT_EQ(5, nb);
  ASSERT_EQ(0xFFFFFF, decoded);
}

TEST(hpack_Encoder, encodeInt8Bit_fit) {
  // encode
  Buffer buf;
  EncoderHelper::encodeInt(&buf, 57, 8);
  BufferRef ref = BufferRef((char*)buf.data(), buf.size());

  // decode
  unsigned nb = 0;
  const uint64_t decoded = DecoderHelper::decodeInt(ref, 8, &nb);

  ASSERT_EQ(57, decoded);
  ASSERT_EQ(1, nb);
}

TEST(hpack_Encoder, encodeInt8Bit_nofit2) {
  // encode
  Buffer buf;
  EncoderHelper::encodeInt(&buf, 356, 8);
  BufferRef ref = BufferRef((char*)buf.data(), buf.size());

  // decode
  unsigned nb = 0;
  const uint64_t decoded = DecoderHelper::decodeInt(ref, 8, &nb);

  ASSERT_EQ(356, decoded);
  ASSERT_EQ(2, nb);
}

TEST(hpack_Encoder, encodeInt8Bit_nofit4) {
  // encode
  Buffer buf;
  EncoderHelper::encodeInt(&buf, 0x12345678llu, 8);

  ASSERT_EQ(6, buf.size());
  ASSERT_EQ('\xFF', buf[0]);
  ASSERT_EQ('\xF9', buf[1]);
  ASSERT_EQ('\xAA', buf[2]);
  ASSERT_EQ('\xD1', buf[3]);
  ASSERT_EQ('\x91', buf[4]);
  ASSERT_EQ('\x01', buf[5]);

  // decode
  BufferRef ref = BufferRef((char*)buf.data(), buf.size());
  unsigned nb = 0;
  const uint64_t decoded = DecoderHelper::decodeInt(ref, 8, &nb);

  ASSERT_EQ(6, nb);
  ASSERT_EQ(0x12345678llu, decoded);
}

// Appending D.1.2) encode 1337 with 5bit prefix
TEST(hpack_Encoder, encodeInt_1337_5bit) {
  Buffer buf;
  EncoderHelper::encodeInt(&buf, 1337, 5);

  ASSERT_EQ(3, buf.size());
  ASSERT_EQ('\x1F', buf[0]);
  ASSERT_EQ('\x9A', buf[1]);
  ASSERT_EQ('\x0A', buf[2]);

  unsigned nb = 0;
  const uint64_t decoded = DecoderHelper::decodeInt(buf, 5, &nb);
  ASSERT_EQ(1337, decoded);
  ASSERT_EQ(3, nb);
}

TEST(hpack_Decoder, example_2)
{
}

// vim:ts=2:sw=2
