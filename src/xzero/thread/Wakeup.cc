// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/thread/Wakeup.h>
#include <chrono>

namespace xzero {

Wakeup::Wakeup() : gen_(0) {
}

void Wakeup::waitForNextWakeup() {
  waitForWakeup(generation());
}

void Wakeup::waitForFirstWakeup() {
  waitForWakeup(0);
}

void Wakeup::waitForWakeup(long oldgen) {
  std::unique_lock<std::mutex> l(mutex_);

  condvar_.wait(l, [this, oldgen] {
    return gen_.load() > oldgen;
  });
}

void Wakeup::waitFor(Duration timeout) {
  waitFor(timeout, generation());
}

void Wakeup::waitFor(Duration timeout, long oldgen) {
  std::unique_lock<std::mutex> l(mutex_);

  std::chrono::milliseconds rel_time(timeout.milliseconds());

  condvar_.wait_for(l, rel_time, [this, oldgen] {
    return gen_.load() > oldgen;
  });
}

void Wakeup::onWakeup(long generation, std::function<void()> callback) {
  std::unique_lock<std::mutex> l(mutex_);

  if (gen_.load() > generation) {
    l.unlock();
    callback();
    return;
  }

  callbacks_.push_back(callback);
}

long Wakeup::generation() const {
  return gen_.load();
}

void Wakeup::wakeup() {
  std::list<std::function<void ()>> callbacks;

  std::unique_lock<std::mutex> l(mutex_);
  gen_++;
  callbacks.splice(callbacks.begin(), callbacks_);
  l.unlock();

  condvar_.notify_all();

  for (const auto& callback: callbacks) {
    callback();
  }
}

} // namespace xzero

