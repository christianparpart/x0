// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/thread/Queue.h>

using namespace xzero;

TEST(Queue, poll) {
  thread::Queue<int> queue;
  std::optional<int> o = queue.poll();
  ASSERT_TRUE(!o);

  queue.insert(42);
  o = queue.poll();
  ASSERT_TRUE(o.has_value());
  ASSERT_EQ(42, o.value());
}

TEST(Queue, pop) {
  thread::Queue<int> queue;
  queue.insert(42);

  // TODO: make sure this was kinda instant
  std::optional<int> o = queue.pop();
  ASSERT_TRUE(o.has_value());
  ASSERT_EQ(42, o.value());

  // TODO: queue.pop() and have a bg thread inserting something 1s later.
  // We expect pop to complete after 1s.
}
