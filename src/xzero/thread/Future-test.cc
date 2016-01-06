// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <gtest/gtest.h>
#include <xzero/thread/Future.h>

using namespace xzero;

Future<int> getSomeFuture(int i) {
  Promise<int> promise;
  promise.success(i);
  return promise.future();
}

TEST(Future, successNow) {
  Promise<int> promise;
  promise.success(42);

  Future<int> f = promise.future();

  ASSERT_TRUE(f.isReady());
  f.wait(); // call wait anyway, for completeness

  ASSERT_TRUE(f.isSuccess());
  ASSERT_FALSE(f.isFailure());
  ASSERT_EQ(42, f.get());
}

TEST(Future, failureGetThrows) {
  Promise<int> promise;
  promise.failure(Status::KeyError);

  Future<int> f = promise.future();

  ASSERT_TRUE(f.isReady());
  f.wait(); // call wait anyway, for completeness

  ASSERT_FALSE(f.isSuccess());
  ASSERT_TRUE(f.isFailure());

  ASSERT_THROW([&]() {
    try {
      f.get();
    } catch (const RuntimeError& e) {
      ASSERT_EQ(Status::KeyError, static_cast<Status>(e.code().value()));
      throw;
    }
  }(), RuntimeError);
}

TEST(Future, testAsyncResult) {
  // TODO FIXME FIXPAUL FIXCHRIS d'oh FIXOVERFLOW
}
