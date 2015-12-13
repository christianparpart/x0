// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/HttpRequestInfo.h>
#include <xzero/Application.h>
#include <xzero/logging.h>
#include <xzero/stdtypes.h>
#include <gtest/gtest.h>

using namespace xzero;
using namespace xzero::http;

TEST(HttpRequestInfo, test_ctor) {
  HttpRequestInfo a(HttpVersion::VERSION_1_1, "GET", "/foo?bar=com", 123, {
        {"Foo", "FooValue"},
        {"Bar", "BarValue"},
      });

  ASSERT_EQ("GET", a.unparsedMethod());
  ASSERT_EQ(HttpMethod::GET, a.method());
  ASSERT_EQ(123, a.contentLength());
  ASSERT_EQ(2, a.headers().size());
  ASSERT_EQ("Foo", a.headers()[0].name());
  ASSERT_EQ("FooValue", a.headers()[0].value());
  ASSERT_EQ("Bar", a.headers()[1].name());
  ASSERT_EQ("BarValue", a.headers()[1].value());
}

TEST(HttpRequestInfo, test_ctor_copy) {
  HttpRequestInfo req(HttpVersion::VERSION_1_1, "GET", "/foo?bar=com", 123, {
        {"Foo", "FooValue"},
        {"Bar", "BarValue"},
      });

  HttpRequestInfo a(req);

  ASSERT_EQ("GET", a.unparsedMethod());
  ASSERT_EQ(HttpMethod::GET, a.method());
  ASSERT_EQ(123, a.contentLength());
  ASSERT_EQ(2, a.headers().size());
  ASSERT_EQ("Foo", a.headers()[0].name());
  ASSERT_EQ("FooValue", a.headers()[0].value());
  ASSERT_EQ("Bar", a.headers()[1].name());
  ASSERT_EQ("BarValue", a.headers()[1].value());
}

TEST(HttpRequestInfo, test_assign_operator) {
  HttpRequestInfo req(HttpVersion::VERSION_1_1, "GET", "/foo?bar=com", 123, {
        {"Foo", "FooValue"},
        {"Bar", "BarValue"},
      });

  HttpRequestInfo a(HttpVersion::VERSION_0_9, "DELETE", "/implode", 0, {});
  a = req;

  ASSERT_EQ("GET", a.unparsedMethod());
  ASSERT_EQ(HttpMethod::GET, a.method());
  ASSERT_EQ(123, a.contentLength());
  ASSERT_EQ(2, a.headers().size());
  ASSERT_EQ("Foo", a.headers()[0].name());
  ASSERT_EQ("FooValue", a.headers()[0].value());
  ASSERT_EQ("Bar", a.headers()[1].name());
  ASSERT_EQ("BarValue", a.headers()[1].value());
}

