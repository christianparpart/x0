// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Option.h>

using xzero::Option;
using xzero::Some;
using xzero::None;

TEST(Option, ctor0) {
  Option<int> x;

  EXPECT_TRUE(x.isNone());
}

TEST(Option, ctorNone) {
  Option<int> x = None(); // invokes the ctor Option(const None&)

  EXPECT_TRUE(x.isNone());
}

struct Movable {
  Movable(int v) : value(v) { }
  Movable(const Movable& m) : value(m.value) { }
  Movable(Movable&& m) : value(m.value) { m.value = -1; }
  Movable& operator=(Movable&& m) {
    value = m.value;
    m.value = -1;
    return *this;
  }
  int value;
};

TEST(Option, ctorMoveValue) {
  Movable i = 42;
  Option<Movable> x = std::move(i);

  EXPECT_TRUE(x.isSome());
  EXPECT_EQ(42, x->value);
  EXPECT_EQ(-1, i.value);
}

TEST(Option, moveAssign) {
  auto a = Some(Movable(42));
  auto b = Some(Movable(13));

  a = std::move(b);

  EXPECT_EQ(13, a.get().value);
  EXPECT_TRUE(b == None());
}

TEST(Option, isNone) {
  Option<int> x = None();

  EXPECT_TRUE(x.isNone());
}

TEST(Option, isSome) {
  Option<int> x = 42;
  EXPECT_FALSE(x.isNone());
  EXPECT_TRUE(x.isSome());
}

TEST(Option, operatorBool) {
  Option<int> x = 42;
  EXPECT_FALSE(!x);
  EXPECT_TRUE(x);
}

TEST(Option, operatorEqu) {
  EXPECT_TRUE(Some(42) == Some(42));
  EXPECT_FALSE(Some(13) == Some(42));
  EXPECT_FALSE(Some(13) == None());
}

TEST(Option, operatorNe) {
  EXPECT_TRUE(Some(13) != Some(42));
  EXPECT_FALSE(Some(42) != Some(42));
  EXPECT_TRUE(Some(13) != None());
}

TEST(Option, getSome) {
  Option<int> x = 42;
  EXPECT_EQ(42, x.get());
}

TEST(Option, getNone) {
  Option<int> x = None();

  EXPECT_ANY_THROW(x.get());
  EXPECT_ANY_THROW(*x);
}

TEST(Option, clear) {
  Option<int> x = 42;

  x.clear();

  EXPECT_TRUE(x.isNone());
}

TEST(Option, onSome) {
  int retrievedValue = 0;

  Some(42).onSome([&](int i) {
    retrievedValue = i;
  }).onNone([&]() {
    retrievedValue = -1;
  });

  EXPECT_EQ(42, retrievedValue);
}

TEST(Option, onNone) {
  int retrievedValue = 0;

  Option<int>().onSome([&](int i) {
    retrievedValue = i;
  }).onNone([&]() {
    retrievedValue = -1;
  });

  EXPECT_EQ(-1, retrievedValue);
}
