// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/ByteArrayEndPoint.h>
#include <xzero/testing.h>

using namespace xzero;

TEST(ByteArrayEndPoint, setInput_cstr) {
  ByteArrayEndPoint ep;
  ep.setInput("foo bar");

  ASSERT_EQ("foo bar", ep.input());
}

TEST(ByteArrayEndPoint, setInput_buf_moved) {
  Buffer input = "foo bar";
  ByteArrayEndPoint ep;
  ep.setInput(std::move(input));

  ASSERT_EQ("foo bar", ep.input());
}

TEST(ByteArrayEndPoint, flush) {
  ByteArrayEndPoint ep;

  ep.flush("foo");
  ASSERT_EQ("foo", ep.output());

  ep.flush(" bar");
  ASSERT_EQ("foo bar", ep.output());
}

TEST(ByteArrayEndPoint, fill) {
  ByteArrayEndPoint ep;
  Buffer input = "foo ";

  ep.setInput("bar");
  ep.fill(&input);
  ASSERT_EQ("foo bar", input);
}

TEST(ByteArrayEndPoint, DISABLED_close) {
  ByteArrayEndPoint ep;
  Buffer output;

  ep.setInput("foo");

  ASSERT_TRUE(ep.isOpen());
  ep.close();
  ASSERT_FALSE(ep.isOpen());

  ssize_t rv = ep.fill(&output);
  ASSERT_EQ(0, rv);
  ASSERT_EQ("", output);

  rv = ep.flush("bar");
  ASSERT_EQ(0, rv);
  ASSERT_EQ("", ep.output());
}
