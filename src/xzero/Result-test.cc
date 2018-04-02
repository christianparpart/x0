// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Result.h>
#include <xzero/fmtutil.h>
#include <xzero/testing.h>

#include <future>
#include <system_error>

TEST(Result, Failure) {
  Result<int> t = std::make_error_code(std::errc::result_out_of_range);

  ASSERT_FALSE(t.isSuccess());
  ASSERT_TRUE(t.isFailure());
  ASSERT_EQ(std::errc::result_out_of_range, t.error());
}

TEST(Result, Success) {
  Result<int> t = Success(42);

  ASSERT_TRUE(t.isSuccess());
  ASSERT_FALSE(t.isFailure());
  ASSERT_EQ(*t, 42);
}

TEST(Result, MoveSuccess) {
  Result<std::string> t = Success(std::string("Hello"));

  Result<std::string> u = std::move(t);

  ASSERT_TRUE(u.isSuccess());
  ASSERT_FALSE(u.isFailure());
  ASSERT_EQ(*u, "Hello");
}

TEST(Result, ErrorEnumClass) {
  Result<int> t = std::errc::io_error;
  ASSERT_EQ(std::errc::io_error, t.error());

  Result<int> u = std::future_errc::no_state;
  ASSERT_EQ(std::future_errc::no_state, u.error());
}

TEST(Result, BadAccess) {
  Result<int> t = std::errc::io_error;
  EXPECT_THROW(t.get(), ResultBadAccess);
}

TEST(Result, CtorEmptyErrorInvalid) {
  static const std::error_code no_error;
  EXPECT_THROW({ Result<int> t(no_error); }, std::invalid_argument);
}
