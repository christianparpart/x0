// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <gtest/gtest.h>
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
