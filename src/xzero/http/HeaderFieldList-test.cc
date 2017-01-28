// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HeaderField.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;

TEST(http_HeaderFieldList, ctors) {
  // ctor()
  HeaderFieldList a;
  ASSERT_TRUE(a.empty());
  ASSERT_EQ(0, a.size());

  // ctor(initializer_list)
  HeaderFieldList b = {
    {"foo", "bar"},
    {"tom", "tar"},
  };
  ASSERT_FALSE(b.empty());
  ASSERT_EQ(2, b.size());

  ASSERT_EQ("bar", b.get("foo"));
  ASSERT_EQ("tar", b.get("tom"));

  // ctor(HeaderFieldList&&);
  HeaderFieldList c(std::move(b));
  ASSERT_EQ(0, a.size());
  ASSERT_EQ(2, c.size());

  // ctor(const HeaderFieldList&);
  HeaderFieldList d(c);
  ASSERT_EQ(2, d.size());
  ASSERT_EQ(2, c.size());

  // operator=(HeaderFieldList&&)
  a = std::move(c);
  ASSERT_EQ(2, a.size());
  ASSERT_EQ(0, c.size());

  // operator=(HeaderFieldList&)
  c = a;
  ASSERT_EQ(2, a.size());
  ASSERT_EQ(2, c.size());
}

TEST(http_HeaderFieldList, push_back) {
  HeaderFieldList a;
  a.push_back("foo", "bar");
  ASSERT_EQ(1, a.size());
  ASSERT_EQ("bar", a.get("foo"));
}

TEST(http_HeaderFieldList, overwrite) {
  HeaderFieldList a;

  // overwrite existing field
  a.push_back("foo", "bar");
  a.overwrite("foo", "tom");
  ASSERT_EQ("tom", a.get("foo"));

  // create new field
  a.overwrite("bar", "tim");
  ASSERT_EQ(2, a.size());
  ASSERT_EQ("tim", a["bar"]);
}

TEST(http_HeaderFieldList, append) {
  HeaderFieldList a;

  a.append("foo", "bar");
  ASSERT_EQ("bar", a["foo"]);

  a.append("foo", "ten");
  ASSERT_EQ("barten", a["foo"]);

  a.append("foo", "er", "d");
  ASSERT_EQ("bartender", a["foo"]);
}

TEST(http_HeaderFieldList, remove) {
  HeaderFieldList a = {{"foo", "bar"}};

  a.remove("foo");
  ASSERT_EQ(0, a.size());
}

TEST(http_HeaderFieldList, contains) {
  HeaderFieldList a = {{"foo", "bar"}};

  ASSERT_TRUE(a.contains("foo"));
  ASSERT_TRUE(a.contains("FOO"));
  ASSERT_TRUE(a.contains("Foo"));

  ASSERT_FALSE(a.contains("tim"));
}

TEST(http_HeaderFieldList, swap) {
  HeaderFieldList a = {{"foo", "bar"}};
  HeaderFieldList b;

  a.swap(b);

  ASSERT_EQ(0, a.size());
  ASSERT_EQ(1, b.size());
  ASSERT_EQ("bar", b.get("foo"));
}

TEST(http_HeaderFieldList, reset) {
  HeaderFieldList a = {{"foo", "bar"}};
  a.reset();

  ASSERT_TRUE(a.empty());
}
