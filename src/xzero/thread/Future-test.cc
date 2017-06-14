// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/thread/Future.h>

using namespace xzero;

Future<int> getSomeFuture(int i) {
  Promise<int> promise;
  promise.success(i);
  return promise.future();
}

Future<double> i2d(int i) {
  Promise<double> promise;
  promise.success(double(i * i));
  return promise.future();
}

Future<void> getVoidFuture() {
  Promise<void> promise;
  promise.success();
  return promise.future();
}

TEST(Future, of_void) {
  Future<void> f = getVoidFuture();
  f.wait();
  EXPECT_TRUE(f.isSuccess());
}

TEST(Future, chain1) {
  Future<int> f = getSomeFuture(42);
  Future<double> g = f.chain(i2d);
  Future<int> h = g.chain([](double x) -> Future<int> {
    Promise<int> promise;
    promise.success(static_cast<int>(x) - 1700);
    return promise.future();
  });
  EXPECT_EQ(42, f.get());
  EXPECT_EQ(1764.0, g.get());
  EXPECT_EQ(64, h.get());
}

TEST(Future, chain2) {
  Future<int> f = getSomeFuture(42).chain(i2d).chain([](double x) -> Future<int> {
    Promise<int> promise;
    promise.success(static_cast<int>(x) - 1700);
    return promise.future();
  });
  EXPECT_EQ(64, f.get());
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
  promise.failure(std::errc::io_error);

  Future<int> f = promise.future();

  ASSERT_TRUE(f.isReady());
  f.wait(); // call wait anyway, for completeness

  ASSERT_FALSE(f.isSuccess());
  ASSERT_TRUE(f.isFailure());

  ASSERT_THROW([&]() {
    try {
      f.get();
    } catch (const RuntimeError& e) {
      ASSERT_EQ(std::errc::io_error, static_cast<std::errc>(e.code().value()));
      throw;
    }
  }(), RuntimeError);
}

TEST(Future, testAsyncResult) {
  // TODO FIXME FIXPAUL FIXCHRIS d'oh FIXOVERFLOW
}
