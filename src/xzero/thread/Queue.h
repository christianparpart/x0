// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <condition_variable>
#include <functional>
#include <optional>
#include <atomic>
#include <deque>

namespace xzero::thread {

/**
 * A queue is threadsafe
 */
template <typename T>
class Queue {
public:
  void insert(const T& job);
  T pop();
  std::optional<T> poll();

protected:
  std::deque<T> queue_;
  std::mutex mutex_;
  std::condition_variable wakeup_;
};

// {{{ inlines
template <typename T>
void Queue<T>::insert(const T& job) {
  std::unique_lock<std::mutex> lk(mutex_);
  queue_.emplace_back(job);
  lk.unlock();
  wakeup_.notify_one();
}

template <typename T>
T Queue<T>::pop() {
  std::unique_lock<std::mutex> lk(mutex_);

  while (queue_.size() == 0) {
    wakeup_.wait(lk);
  }

  auto job = queue_.front();
  queue_.pop_front();
  return job;
}

template <typename T>
std::optional<T> Queue<T>::poll() {
  std::unique_lock<std::mutex> lk(mutex_);

  if (queue_.size() == 0) {
    return std::nullopt;
  } else {
    auto job = queue_.front();
    queue_.pop_front();
    return job;
  }
}
// }}}

} // namespace xzero::thread
