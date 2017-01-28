// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Buffer.h>
#include <xzero/base64.h>
#include <cstdio>

using namespace xzero;

TEST(Base64, encode) {
  ASSERT_EQ("YQ==", base64::encode("a"));
  ASSERT_EQ("YWI=", base64::encode("ab"));
  ASSERT_EQ("YWJj", base64::encode("abc"));
  ASSERT_EQ("YWJjZA==", base64::encode("abcd"));
  ASSERT_EQ("Zm9vOmJhcg==", base64::encode("foo:bar"));
}

TEST(Base64, decode) {
  ASSERT_EQ("a", base64::decode("YQ=="));
  ASSERT_EQ("ab", base64::decode("YWI="));
  ASSERT_EQ("abc", base64::decode("YWJj"));
  ASSERT_EQ("abcd", base64::decode("YWJjZA=="));
  ASSERT_EQ("foo:bar", base64::decode("Zm9vOmJhcg=="));
}
