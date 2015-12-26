// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <gtest/gtest.h>
#include <xzero/http/http2/Generator.h>
#include <xzero/net/ByteArrayEndPoint.h>
#include <xzero/net/EndPointWriter.h>

using namespace xzero;
using namespace xzero::http::http2;

TEST(http_http2_Generator, settings) {
  EndPointWriter writer;
  Generator generator(&writer);

  generator.generateSettings({
      {SettingParameter::EnablePush, 1},
      {SettingParameter::MaxConcurrentStreams, 16},
      {SettingParameter::InitialWindowSize, 42},
  });
  generator.flushBuffer();

  ByteArrayEndPoint endpoint;
  writer.flush(&endpoint);

  ASSERT_EQ(9 + 3 * 6, endpoint.output().size());
  // TODO: some binary comparison of what we expect
}

TEST(http_http2_Generator, settingsAck) {
  EndPointWriter writer;
  Generator generator(&writer);

  generator.generateSettingsAcknowledgement();
  generator.flushBuffer();

  ByteArrayEndPoint endpoint;
  writer.flush(&endpoint);

  ASSERT_EQ(9, endpoint.output().size());
  // TODO: some binary comparison of what we expect
}
