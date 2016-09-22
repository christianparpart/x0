// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/IdleTimeout.h>
#include <xzero/logging.h>
#include <xzero/MonotonicClock.h>
#include <xzero/MonotonicTime.h>
#include <xzero/executor/Executor.h>
#include <assert.h>

namespace xzero {

#define ERROR(msg...) do { logError("IdleTimeout", msg); } while (0)
#ifndef NDEBUG
#define TRACE(msg...) do { logTrace("IdleTimeout", msg); } while (0)
#else
#define TRACE(msg...) do {} while (0)
#endif

IdleTimeout::IdleTimeout(Executor* executor,
                         Duration timeout,
                         Executor::Task cb)
    : executor_(executor),
      timeout_(timeout),
      fired_(),
      active_(false),
      onTimeout_(cb) {
}

IdleTimeout::IdleTimeout(Executor* executor) 
    : IdleTimeout(executor, Duration::Zero, nullptr) {
}

IdleTimeout::~IdleTimeout() {
}

void IdleTimeout::setTimeout(Duration value) {
  timeout_ = value;
}

Duration IdleTimeout::timeout() const {
  return timeout_;
}

void IdleTimeout::setCallback(std::function<void()> cb) {
  onTimeout_ = cb;
}

void IdleTimeout::clearCallback() {
  onTimeout_ = decltype(onTimeout_)();
}

void IdleTimeout::touch() {
  if (isActive()) {
    schedule();
  }
}

void IdleTimeout::activate() {
  assert(onTimeout_ && "No timeout callback defined");
  if (!active_) {
    active_ = true;
    schedule();
  }
}

void IdleTimeout::deactivate() {
  active_ = false;
}

Duration IdleTimeout::elapsed() const {
  if (isActive()) {
    return MonotonicClock::now() - fired_;
  } else {
    return Duration::Zero;
  }
}

void IdleTimeout::reschedule() {
  assert(isActive());

  handle_->cancel();

  Duration deltaTimeout = timeout_ - (MonotonicClock::now() - fired_);
  handle_ = executor_->executeAfter(deltaTimeout,
                                     std::bind(&IdleTimeout::onFired, this));
}

void IdleTimeout::schedule() {
  fired_ = MonotonicClock::now();

  if (handle_)
    handle_->cancel();

  handle_ = executor_->executeAfter(timeout_,
                                    std::bind(&IdleTimeout::onFired, this));
}

void IdleTimeout::onFired() {
  TRACE("IdleTimeout($0).onFired: active=$1", this, active_);
  if (!active_) {
    return;
  }

  if (elapsed() >= timeout_) {
    active_ = false;
    onTimeout_();
  } else if (isActive()) {
    reschedule();
  }
}

bool IdleTimeout::isActive() const {
  return active_;
}

template<>
std::string StringUtil::toString(IdleTimeout& timeout) {
  return StringUtil::format("IdleTimeout[$0]", timeout.timeout());
}

template<>
std::string StringUtil::toString(IdleTimeout* timeout) {
  return StringUtil::format("IdleTimeout[$0]", timeout->timeout());
}

} // namespace xzero
