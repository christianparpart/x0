// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/DeadlineTimer.h>
#include <xzero/logging.h>
#include <xzero/MonotonicClock.h>
#include <xzero/MonotonicTime.h>
#include <xzero/executor/Executor.h>
#include <cassert>

namespace xzero {

DeadlineTimer::DeadlineTimer(Executor* executor,
                             Executor::Task cb,
                             Duration timeout)
    : executor_{executor},
      timeout_{timeout},
      fired_{},
      active_{false},
      onTimeout_{std::move(cb)} {
}

DeadlineTimer::DeadlineTimer(Executor* executor, Executor::Task cb)
    : DeadlineTimer{executor, std::move(cb), Duration::Zero} {
}

DeadlineTimer::DeadlineTimer(Executor* executor) 
    : DeadlineTimer{executor, nullptr, Duration::Zero} {
}

void DeadlineTimer::setTimeout(Duration value) {
  timeout_ = value;
}

Duration DeadlineTimer::timeout() const {
  return timeout_;
}

void DeadlineTimer::setCallback(std::function<void()> cb) {
  onTimeout_ = std::move(cb);
}

void DeadlineTimer::clearCallback() {
  onTimeout_ = decltype(onTimeout_)();
}

void DeadlineTimer::touch() {
  if (isActive()) {
    schedule();
  }
}

void DeadlineTimer::start() {
  assert(onTimeout_ && "No timeout callback defined");
  bool cur = active_.load();
  if (!cur && active_.compare_exchange_strong(cur, true,
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) {
    schedule();
  }
}

void DeadlineTimer::rewind() {
  assert(onTimeout_ && "No timeout callback defined");
  if (!active_) {
    active_ = true;
  }
  schedule();
}

void DeadlineTimer::cancel() {
  if (handle_) {
    handle_->cancel();
  }

  active_ = false;
}

Duration DeadlineTimer::elapsed() const {
  if (isActive()) {
    return MonotonicClock::now() - fired_;
  } else {
    return Duration::Zero;
  }
}

void DeadlineTimer::reschedule() {
  assert(isActive());

  handle_->cancel();

  Duration deltaTimeout = timeout_ - (MonotonicClock::now() - fired_);
  handle_ = executor_->executeAfter(deltaTimeout,
                                     std::bind(&DeadlineTimer::onFired, this));
}

void DeadlineTimer::schedule() {
  fired_ = MonotonicClock::now();

  if (handle_)
    handle_->cancel();

  handle_ = executor_->executeAfter(timeout_,
                                    std::bind(&DeadlineTimer::onFired, this));
}

void DeadlineTimer::onFired() {
  if (!isActive()) {
    return;
  }

  if (elapsed() >= timeout_) {
    active_.store(false);
    onTimeout_();
  } else if (isActive()) {
    reschedule();
  }
}

bool DeadlineTimer::isActive() const {
  return active_.load();
}

} // namespace xzero
