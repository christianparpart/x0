// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpRequestInfo.h>
#include <xzero/Application.h>
#include <xzero/logging.h>
#include <xzero/testing.h>

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

