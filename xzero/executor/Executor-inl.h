// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <functional>
#include <mutex>

namespace xzero {

inline void Executor::Handle::reset(Task onCancel) {
  std::lock_guard<std::mutex> lk(mutex_);

  isCancelled_.store(false);
  onCancel_ = onCancel;
}

inline bool Executor::Handle::isCancelled() const {
  return isCancelled_.load();
}

inline void Executor::Handle::setCancelHandler(Task task) {
  std::lock_guard<std::mutex> lk(mutex_);
  onCancel_ = task;
}

inline void Executor::Handle::cancel() {
  std::lock_guard<std::mutex> lk(mutex_);

  for (;;) {
    auto isCancelled = isCancelled_.load();
    if (isCancelled)
      // already cancelled
      break;

    if (isCancelled_.compare_exchange_strong(isCancelled, true)) {
      if (onCancel_) {
        auto cancelThat = std::move(onCancel_);
        cancelThat();
      }
      break;
    }
  }
}

inline void Executor::Handle::fire(Task task) {
  std::unique_lock<std::mutex> lk(mutex_);

  if (!isCancelled_.load()) {
    lk.unlock();
    task();
  }
}

inline Executor::HandleRef Executor::executeOnReadable(int fd, Task task) {
  return executeOnReadable(fd, task, 5_years, nullptr);
}

inline Executor::HandleRef Executor::executeOnWritable(int fd, Task task) {
  return executeOnWritable(fd, task, 5_years, nullptr);
}

inline int Executor::referenceCount() const noexcept {
  return refs_.load();
}

inline void Executor::ref() {
  refs_++;
}

inline void Executor::unref() {
  refs_--;
}

inline void Executor::unref(int count) {
  refs_ -= count;
}

}  // namespace xzero
