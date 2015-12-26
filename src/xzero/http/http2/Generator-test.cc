// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <gtest/gtest.h>
#include <xzero/http/http2/Generator.h>
#include <xzero/io/DataChainListener.h>
#include <xzero/io/DataChain.h>
#include <xzero/Application.h>

using namespace xzero;
using namespace xzero::http::http2;

TEST(http_http2_Generator, data_single_frame) {
  Application::logToStderr(LogLevel::Trace);

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
  DataChain chain;
  Generator generator(&chain);
  generator.setMaxFrameSize(6); // XXX 16384 is actually the minimum allowed

  generator.generateData(42, "Hello World", true);

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(29, sink.size());
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

  generator.generateSettingsAcknowledgement();

  Buffer sink;
  chain.transferTo(&sink);

  ASSERT_EQ(9, sink.size());
  // TODO: some binary comparison of what we expect
}
