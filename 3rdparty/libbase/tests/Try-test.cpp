// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/Try.h>
#include <gtest/gtest.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

using namespace base;

TEST(Try, ctor0) {
  Try<int> m;

  ASSERT_TRUE(m.isOkay());
  ASSERT_FALSE(m.isError());
}

TEST(Try, ctor1) {
  Try<int> m = 42;

  ASSERT_TRUE(m.isOkay());
  ASSERT_FALSE(m.isError());

  ASSERT_EQ(42, m.get());
}

TEST(Try, error) {
  Try<int> m = Error("blah");

  ASSERT_FALSE(m.isOkay());
  ASSERT_TRUE(m.isError());
}

TEST(Try, clear) {
  Try<int> m = Error("blah");
  m.clear();

  ASSERT_TRUE(m.isOkay());
}

TEST(Try, copy) {
  Try<int> a;
  Try<int> b = 42;

  // copy value
  a = b;

  ASSERT_TRUE(a.isOkay());
  ASSERT_TRUE(b.isOkay());

  ASSERT_EQ(42, a.get());
  ASSERT_EQ(42, b.get());

  // copy error
  b = Error("blah");
  a = b;

  ASSERT_TRUE(a.isError());
  ASSERT_EQ("blah", a.errorMessage());

  ASSERT_TRUE(b.isError());
  ASSERT_EQ("blah", b.errorMessage());
}

TEST(Try, move) {
  Try<int> a = Error("blah");
  Try<int> b = 42;

  // move value
  a = std::move(b);

  ASSERT_TRUE(a.isOkay());
  ASSERT_EQ(42, a.get());

  // move error
  b = Error("blah");
  a = std::move(b);

  ASSERT_TRUE(a.isError());
  ASSERT_EQ("blah", a.errorMessage());
}

TEST(Try, memberAccess) {
  std::string hello = "hello";
  Try<std::string> m = hello;

  ASSERT_EQ("hello", m.get());
  ASSERT_EQ(5, m->size());
}

TEST(Try, equality) {
  Try<int> a = 42;
  Try<int> b = 42;

  ASSERT_TRUE(a == a);
  ASSERT_FALSE(a != b);

  b = Error("blah");
  ASSERT_FALSE(a == b);
  ASSERT_TRUE(a != b);

  b = 17;
  ASSERT_FALSE(a == b);
  ASSERT_TRUE(a != b);
}

TEST(Try, onOkay) {
  int inner = 0;

  Try<int>(42).onOkay([&](int i) { inner = i; }).onError([&](
      const char* errorMessage) { inner = 2; });

  ASSERT_EQ(42, inner);
}

TEST(Try, onError) {
  int inner = 0;

  Try<int>(Error("blah")).onOkay([&](int i) { inner = i; }).onError([&](
      const char* errorMessage) {
    inner = 2;
    ASSERT_EQ("blah", errorMessage);
  });

  ASSERT_EQ(2, inner);
}
