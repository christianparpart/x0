// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <gtest/gtest.h>
#include <x0/Maybe.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

using namespace x0;

TEST(Maybe, none) {
  Maybe<int> m;

  ASSERT_TRUE(m.isNone());
  ASSERT_FALSE(m.isSome());
}

TEST(Maybe, some) {
  Maybe<int> m = 42;

  ASSERT_TRUE(m.isSome());
  ASSERT_FALSE(m.isNone());

  ASSERT_EQ(42, m.get());
}

TEST(Maybe, clear) {
  Maybe<int> m = 42;
  m.clear();

  ASSERT_TRUE(m.isNone());
}

TEST(Maybe, copy) {
  Maybe<int> a;
  Maybe<int> b = 42;

  a = b;

  ASSERT_TRUE(a.isSome());
  ASSERT_TRUE(b.isSome());

  ASSERT_EQ(42, a.get());
  ASSERT_EQ(42, b.get());
}

TEST(Maybe, move) {
  Maybe<int> a;
  Maybe<int> b = 42;

  a = std::move(b);

  ASSERT_TRUE(a.isSome());
  ASSERT_TRUE(b.isNone());

  ASSERT_EQ(42, a.get());
}

TEST(Maybe, getOrElse) {
  Maybe<int> a = 42;
  ASSERT_EQ(42, a.getOrElse(-1));

  a.clear();
  ASSERT_EQ(-1, a.getOrElse(-1));
}

TEST(Maybe, memberAccess) {
  std::string hello = "hello";
  Maybe<std::string> m = hello;

  ASSERT_EQ("hello", m.get());
  ASSERT_EQ(5, m->size());
}

TEST(Maybe, iterNone) {
  Maybe<int> m;
  size_t c = 0;
  for (auto i : m) {
    c += i;
  }
  ASSERT_EQ(0, c);
  ASSERT_TRUE(m.begin() == m.end());
}

TEST(Maybe, iterSome) {
  Maybe<int> m = 42;
  size_t c = 0;
  for (auto i : m) {
    c += i;
  }
  ASSERT_EQ(42, c);
  ASSERT_TRUE(m.begin() != m.end());
}

TEST(Maybe, equality) {
  Maybe<int> a = 42;
  Maybe<int> b = 17;

  ASSERT_TRUE(a == a);
  ASSERT_TRUE(a != b);

  b.clear();
  ASSERT_TRUE(a != b);

  a.clear();
  ASSERT_TRUE(a == b);
}

TEST(Maybe, maybe_if) {
  Maybe<int> m = 42;
  int inner = 0;

  bool result = maybe_if(m, [&](int i) { inner = i; })
                    .otherwise([&]() { inner = 2; })
                    .get();

  ASSERT_TRUE(result);
  ASSERT_EQ(42, inner);
}

TEST(Maybe, maybe_if_not) {
  Maybe<int> m;
  int inner = 0;

  bool result = maybe_if(m, [&](int i) { inner = -i; })
                    .otherwise([&]() { inner = 42; })
                    .get();

  ASSERT_FALSE(result);
  ASSERT_EQ(42, inner);
}
