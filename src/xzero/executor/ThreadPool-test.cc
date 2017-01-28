// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/ThreadPool.h>
#include <xzero/MonotonicClock.h>
#include <xzero/MonotonicTime.h>
#include <xzero/testing.h>
#include <unistd.h> // usleep()

using namespace xzero;

TEST(ThreadPoolTest, simple) {
  xzero::ThreadPool tp(2);
  bool e1 = false;
  bool e2 = false;
  bool e3 = false;

  MonotonicTime startTime;
  MonotonicTime end1;
  MonotonicTime end2;
  MonotonicTime end3;

  startTime = MonotonicClock::now();

  tp.execute([&]() {
    usleep(100_milliseconds .microseconds());
    e1 = true;
    end1 = MonotonicClock::now();
  });

  tp.execute([&]() {
    usleep(100_milliseconds .microseconds());
    e2 = true;
    end2 = MonotonicClock::now();
  });

  tp.execute([&]() {
    usleep(100_milliseconds .microseconds());
    e3 = true;
    end3 = MonotonicClock::now();
  });

  tp.wait(); // wait until all pending & active tasks have been executed

  EXPECT_EQ(true, e1);
  EXPECT_EQ(true, e1);
  EXPECT_EQ(true, e3); // FIXME: where is the race, why does this fail sometimes

  EXPECT_NEAR(100, (end1 - startTime).milliseconds(), 10); // executed instantly
  EXPECT_NEAR(100, (end2 - startTime).milliseconds(), 10); // executed instantly
  EXPECT_NEAR(200, (end3 - startTime).milliseconds(), 10); // executed queued
}
