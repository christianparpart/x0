// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <gtest/gtest.h>
#include <base/Signal.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

using namespace base;

TEST(Signal, empty) {
  Signal<void(int)> s;
  ASSERT_TRUE(s.empty());
  ASSERT_EQ(0, s.size());
}

TEST(Signal, one) {
  Signal<void(int)> s;
  int o = 0;

  s.connect([&](int i) { o += i; });

  s(42);

  ASSERT_EQ(42, o);
}

TEST(Signal, oneVar) {
  Signal<void(int)> s;
  int o = 0;

  s.connect([&](int i) { o += i; });

  int i = rand();
  s.fire(i);

  ASSERT_TRUE(i == o);
}

TEST(Signal, two) {
  Signal<void(int)> s;
  int o = 0;

  s.connect([&](int i) { o += i; });
  s.connect([&](int i) { o += i; });

  s(42);

  ASSERT_EQ(84, o);
}

TEST(Signal, disconnect) {
  Signal<void(int)> s;
  int o = 0;

  auto c = s.connect([&](int i) { o += i; });
  s(42);
  s.disconnect(c);

  ASSERT_EQ(0, s.size());

  // upon invokation there shall be no affect
  s(42);
  ASSERT_EQ(42, o);
}

TEST(Signal, clear) {
  Signal<void()> s;

  s.connect([](){});
  s.connect([](){});
  s.clear();

  ASSERT_EQ(0, s.size());
}

TEST(Signal, move) {
  Signal<void()> s;
  auto c = s.connect([]() {});

  Signal<void()> t;
  t = std::move(s);

  ASSERT_EQ(0, s.size());
  ASSERT_EQ(1, t.size());

  t.disconnect(c);
  ASSERT_EQ(0, t.size());
}
