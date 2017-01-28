// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Callback.h>
#include <xzero/testing.h>

using namespace xzero;

TEST(Callback, empty) {
  Callback<void(int)> s;

  ASSERT_TRUE(s.empty());
  ASSERT_EQ(0, s.size());

  s(42);
}

TEST(Callback, one) {
  Callback<void(int)> s;
  int invokationValue = 0;

  auto c = s.connect([&](int i) { invokationValue += i; });
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(1, s.size());

  s(42);
  ASSERT_EQ(42, invokationValue);

  s(5);
  ASSERT_EQ(47, invokationValue);

  // remove callback
  s.disconnect(c);
  ASSERT_TRUE(s.empty());
  ASSERT_EQ(0, s.size());
}
