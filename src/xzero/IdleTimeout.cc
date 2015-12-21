// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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

IdleTimeout::IdleTimeout(Executor* executor) :
  executor_(executor),
  timeout_(Duration::Zero),
  fired_(),
  active_(false),
  onTimeout_() {
}

IdleTimeout::~IdleTimeout() {
}

void IdleTimeout::setTimeout(Duration value) {
  timeout_ = value;
}

Duration IdleTimeout::timeout() const {
  return timeout_;
}

void IdleTimeout::setCallback(std::function<void()>&& cb) {
  onTimeout_ = std::move(cb);
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
