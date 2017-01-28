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
  Option<int> o = queue.poll();
  ASSERT_TRUE(o.isNone());

  queue.insert(42);
  o = queue.poll();
  ASSERT_TRUE(o.isSome());
  ASSERT_EQ(42, o.get());
}

TEST(Queue, pop) {
  thread::Queue<int> queue;
  queue.insert(42);

  // TODO: make sure this was kinda instant
  Option<int> o = queue.pop();
  ASSERT_TRUE(o.isSome());
  ASSERT_EQ(42, o.get());

  // TODO: queue.pop() and have a bg thread inserting something 1s later.
  // We expect pop to complete after 1s.
}
