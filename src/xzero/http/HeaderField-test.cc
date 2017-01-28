// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HeaderField.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;

TEST(http_HeaderField, appendValue) {
  HeaderField foo("foo", "bar");

  foo.appendValue("ten");
  ASSERT_EQ("barten", foo.value());

  foo.appendValue("er", "d");
  ASSERT_EQ("bartender", foo.value());
}

TEST(http_HeaderField, operator_EQU) {
  const HeaderField foo("foo", "bar");

  ASSERT_EQ(HeaderField("foo", "bar"), foo);
  ASSERT_EQ(HeaderField("foo", "BAR"), foo);
  ASSERT_EQ(HeaderField("FOO", "BAR"), foo);
}

TEST(http_HeaderField, operator_NE) {
  const HeaderField foo("foo", "bar");

  ASSERT_NE(HeaderField("foo", " bar "), foo);
  ASSERT_NE(HeaderField("foo", "tom"), foo);
  ASSERT_NE(HeaderField("tom", "tom"), foo);
}

TEST(http_HeaderField, inspect) {
  HeaderField field("foo", "bar");
  ASSERT_EQ("HeaderField(\"foo\", \"bar\")", inspect(field));
}
