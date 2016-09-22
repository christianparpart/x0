// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/IdleTimeout.h>
#include <xzero/executor/PosixScheduler.h>

using namespace xzero;

TEST(IdleTimeout, empty) {
  PosixScheduler executor;
  IdleTimeout t(&executor);

  EXPECT_FALSE(t.isActive());
}

TEST(IdleTimeout, simple1) {
  int fired = 0;
  PosixScheduler executor;
  IdleTimeout t(&executor);
  t.setTimeout(500_milliseconds);
  t.setCallback([&]() { fired++; });
  t.activate();
  EXPECT_TRUE(t.isActive());

  executor.runLoop();
  EXPECT_EQ(1, fired);
  EXPECT_FALSE(t.isActive());
}
