// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/net/ByteArrayEndPoint.h>
#include <gtest/gtest.h>

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

  ASSERT_EQ(true, ep.isOpen());
  ep.close();
  ASSERT_EQ(false, ep.isOpen());

  ssize_t rv = ep.fill(&output);
  ASSERT_EQ(0, rv);
  ASSERT_EQ("", output);

  rv = ep.flush("bar");
  ASSERT_EQ(0, rv);
  ASSERT_EQ("", ep.output());
}
