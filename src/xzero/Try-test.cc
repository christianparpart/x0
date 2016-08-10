// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Try.h>

TEST(Try, Failure) {
  Try<int> t = Failure("fnord");

  ASSERT_FALSE(t.isSuccess());
  ASSERT_TRUE(t.isFailure());
}

TEST(Try, Success) {
  Try<int> t = Success(42);

  ASSERT_TRUE(t.isSuccess());
  ASSERT_FALSE(t.isFailure());
  ASSERT_EQ(*t, 42);
}

TEST(Try, MoveSuccess) {
  Try<std::string> t = Success(std::string("Hello"));

  Try<std::string> u = std::move(t);

  ASSERT_TRUE(u.isSuccess());
  ASSERT_FALSE(u.isFailure());
  ASSERT_EQ(*u, "Hello");
  ASSERT_EQ(*t, "");
}
