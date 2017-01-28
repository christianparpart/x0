// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/DeadlineTimer.h>
#include <xzero/executor/PosixScheduler.h>

using namespace xzero;

TEST(DeadlineTimer, empty) {
  PosixScheduler executor;
  DeadlineTimer t(&executor);

  EXPECT_FALSE(t.isActive());
}

TEST(DeadlineTimer, simple1) {
  int fired = 0;
  PosixScheduler executor;
  DeadlineTimer t(&executor);
  t.setTimeout(500_milliseconds);
  t.setCallback([&]() { fired++; });
  t.start();
  EXPECT_TRUE(t.isActive());

  executor.runLoop();
  EXPECT_EQ(1, fired);
  EXPECT_FALSE(t.isActive());
}
