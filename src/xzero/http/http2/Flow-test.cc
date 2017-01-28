// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Flow.h>
#include <xzero/testing.h>

using xzero::http::http2::Flow;

TEST(http_http2_Flow, charge) {
  Flow flow(0);
  ASSERT_EQ(0, flow.available());

  flow.charge(16);
  ASSERT_EQ(16, flow.available());
}

TEST(http_http2_Flow, take) {
  Flow flow(16);
  flow.take(4);
  ASSERT_EQ(12, flow.available());
}

TEST(http_http2_Flow, overcharge) {
  Flow flow(Flow::MaxValue - 1);
  flow.charge(2);
  ASSERT_EQ(Flow::MaxValue - 1, flow.available());
}

TEST(http_http2_Flow, overtake) {
  Flow flow(16);
  flow.take(32);
  ASSERT_EQ(16, flow.available());
}
