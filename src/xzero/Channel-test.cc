// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/testing.h>
#include <xzero/Channel.h>
#include <xzero/MonotonicClock.h>
#include <xzero/Duration.h>
#include <thread>

using xzero::Channel;
using xzero::MonotonicClock;
using xzero::MonotonicTime;
using xzero::Duration;

void go(std::function<void()> fn) {
  std::thread t(fn);
  t.detach();
}

TEST(Channel, receiveOnClosed) {
  Channel<int> c;
  c.close();
  ASSERT_TRUE(c.isClosed());

  int v;
  bool r1 = c.receive(&v);

  ASSERT_FALSE(r1);
}

TEST(Channel, buffered1) {
  Channel<int, 1> c;

  ASSERT_EQ(0, c.size());

  log("c.send(42)");
  bool s1 = c.send(42);
  ASSERT_TRUE(s1);
  ASSERT_EQ(1, c.size());

  int v;
  c.receive(&v);
  logf("v: $0", v);
  ASSERT_EQ(42, v);

  c.send(13);

  bool r2 = c.receive(&v);
  logf("v: $0", v);
  ASSERT_EQ(13, v);
  ASSERT_EQ(0, c.size());

  c.close();
  ASSERT_TRUE(c.isClosed());

  bool r3 = c.receive(&v);
  ASSERT_FALSE(r3); // channel empty
  ASSERT_EQ(0, c.size());

  log("c.close()");
}

TEST(Channel, spam) {
  Channel<unsigned, 100> c;
  MonotonicTime c1 = MonotonicClock::now();
  std::thread([&]() {
    for (unsigned i = 0; i <= 1 << 15; ++i) {
      c.send(i);
    }
    c.close();
  }).detach();

  unsigned v;
  while (c.receive(&v)) {
    ;
  }

  MonotonicTime c2 = MonotonicClock::now();
  Duration d = c2 - c1;
  logf("duration: $0 (last received value: $1)", d, v);
}

TEST(Channel, unbuffered) {
  Channel<int> c;
  ASSERT_EQ(0, c.size());

  std::thread([&]() {
    log("c.send(42)");
    c.send(42);
    log("done");
    c.close();
  }).detach();

  int i1, i2;

  log("c.receive");
  bool closed1 = !c.receive(&i1);

  log("c.receive");
  bool closed2 = !c.receive(&i2);
  log("done");

  ASSERT_FALSE(closed1);
  ASSERT_EQ(42, i1);
  ASSERT_TRUE(closed2);
}

#if 1 == 0
int example_main() {
  Channel<int> a;
  Channel<int> b;

  //...

  int r = select(a, b);
  puts("%v has something ready", r);

  unsigned waiting = 0;
  for (bool quit = false; !quit;) {
    channelSelector()
      .on(a, [&](auto&& v, bool closed) { logf("a: $0", v); quit = closed; })
      .on(b, [&](auto&& v, bool closed) { logf("b: $0", v); quit = closed; });
      .otherwise([&]() { waiting++; });
  }
  logf("wait count: $0", waiting);
}
#endif
