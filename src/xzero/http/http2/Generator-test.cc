// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/http/http2/Generator.h>
#include <xzero/http/http2/ErrorCode.h>
#include <xzero/io/DataChain.h>

using namespace xzero;
using namespace xzero::http::http2;

// {{{ helper
inline uint32_t read24(const BufferRef& buf, size_t offset) {
  uint32_t value = 0;

  assert(offset + 3 <= buf.size());

  value |= (static_cast<uint64_t>(buf[offset + 0]) & 0xFF) << 16;
  value |= (static_cast<uint64_t>(buf[offset + 1]) & 0xFF) << 8;
  value |= (static_cast<uint64_t>(buf[offset + 2]) & 0xFF);

  return value;
}

inline uint64_t read64(const BufferRef& buf, size_t offset) {
  uint64_t value = 0;

  assert(offset + 8 <= buf.size());

  value |= (static_cast<uint64_t>(buf[offset + 0]) & 0xFF) << 56;
  value |= (static_cast<uint64_t>(buf[offset + 1]) & 0xFF) << 48;
  value |= (static_cast<uint64_t>(buf[offset + 2]) & 0xFF) << 40;
  value |= (static_cast<uint64_t>(buf[offset + 3]) & 0xFF) << 32;
  value |= (static_cast<uint64_t>(buf[offset + 4]) & 0xFF) << 24;
  value |= (static_cast<uint64_t>(buf[offset + 5]) & 0xFF) << 16;
  value |= (static_cast<uint64_t>(buf[offset + 6]) & 0xFF) << 8;
  value |= (static_cast<uint64_t>(buf[offset + 7]) & 0xFF);

  return value;
}
// }}}

TEST(http_http2_Generator, data_single_frame) {
  DataChain chain;
  Generator generator(&chain);

  generator.generateData(42, "Hello World", true);

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(20, sink.size());

  // frame length
  EXPECT_EQ(0, (int)sink[0]);
  EXPECT_EQ(0, (int)sink[1]);
  EXPECT_EQ(11, (int)sink[2]);

  // frame type
  EXPECT_EQ(FrameType::Data, ((FrameType) sink[3]));

  // flags: END_STREAM
  EXPECT_EQ(0x01, sink[4]);

  // stream ID
  EXPECT_EQ(0, sink[5]);
  EXPECT_EQ(0, sink[6]);
  EXPECT_EQ(0, sink[7]);
  EXPECT_EQ(42, sink[8]);

  // payload
  EXPECT_EQ("Hello World", sink.ref(9));
}

TEST(http_http2_Generator, data_split_frames) {
  static constexpr size_t InitialMaxFrameSize = 16384;
  DataChain chain;
  Generator generator(&chain);

  Buffer payload(InitialMaxFrameSize, 'x');
  payload.push_back('y'); // force to exceed MAX_FRAME_SIZE

  generator.generateData(42, payload, true);

  Buffer sink;
  chain.transferTo(&sink);

  // expect 2 frames (16384 + 1 + 2*9 = 16403)
  // - frame 1: 9 bytes header + 16384 payload
  // - frame 2: 9 bytes header + 1 byte payload

  ASSERT_EQ(16403, sink.size());
  ASSERT_EQ('y', sink[16402]);
}

// TODO: header...

TEST(http_http2_Generator, priority) {
  DataChain chain;
  Generator generator(&chain);

  generator.generatePriority(42, true, 28, 256);

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(14, sink.size());

  // dependant stream ID + E-bit
  EXPECT_EQ((uint8_t) (1<<7), (uint8_t) sink[9]); // Exclusive-bit set
  EXPECT_EQ(0, sink[10]);
  EXPECT_EQ(0, sink[11]);
  EXPECT_EQ(28, sink[12]);

  // weight
  EXPECT_EQ((uint8_t) 255, (uint8_t) sink[13]);
}

TEST(http_http2_Generator, reset_stream) {
  DataChain chain;
  Generator generator(&chain);

  generator.generateResetStream(42, ErrorCode::EnhanceYourCalm);

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(13, sink.size()); // 9 + 4
  ASSERT_EQ(4, read24(sink, 0)); // payload size

  // ErrorCode (32 bit)
  EXPECT_EQ(0, sink[9]);
  EXPECT_EQ(0, sink[10]);
  EXPECT_EQ(0, sink[11]);
  EXPECT_EQ(11, sink[12]); // 11 = EnhanceYourCalm

}

TEST(http_http2_Generator, settings) {
  DataChain chain;
  Generator generator(&chain);

  generator.generateSettings({
      {SettingParameter::EnablePush, 1},
      {SettingParameter::MaxConcurrentStreams, 16},
      {SettingParameter::InitialWindowSize, 42},
  });

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(9 + 3 * 6, sink.size());

  // TODO: some binary comparison of what we expect
}

TEST(http_http2_Generator, settingsAck) {
  DataChain chain;
  Generator generator(&chain);

  generator.generateSettingsAck();

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(9, sink.size());
  // TODO: some binary comparison of what we expect
}

TEST(http_http2_Generator, ping) {
  DataChain chain;
  Generator generator(&chain);

  generator.generatePing("pingpong");

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(17, sink.size());                               // packet size
  ASSERT_EQ(8, read24(sink, 0));                            // payload size
  ASSERT_EQ((uint8_t) FrameType::Ping, (uint8_t) sink[3]);  // type
  ASSERT_EQ((uint8_t) 0x00, (uint8_t) sink[4]);             // flags
  ASSERT_EQ("pingpong", sink.ref(9));                       // payload
}

TEST(http_http2_Generator, pingAck) {
  DataChain chain;
  Generator generator(&chain);

  generator.generatePingAck("Welcome!");

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(17, sink.size());                               // packet size
  ASSERT_EQ(8, read24(sink, 0));                            // payload size
  ASSERT_EQ((uint8_t) FrameType::Ping, (uint8_t) sink[3]);  // type
  ASSERT_EQ((uint8_t) 0x01, (uint8_t) sink[4]);             // flags
  ASSERT_EQ("Welcome!", sink.ref(9));                       // payload
}
