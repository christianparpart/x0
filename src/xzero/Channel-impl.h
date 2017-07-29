// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <cassert>

namespace xzero {

template<typename T, const size_t BufSize>
Channel<T, BufSize>::Channel()
    : lock_(),
      queue_(),
      isClosed_(false),
      receiversCond_(),
      sendersCond_() {
}

template<typename T, const size_t BufSize>
Channel<T, BufSize>::~Channel() {
  assert(empty() && "Channel must be empty when destructing.");
}

template<typename T, const size_t BufSize>
void Channel<T, BufSize>::close() {
  std::unique_lock<std::mutex> lock(lock_);
  sendersCond_.wait(lock, [this]() { return isClosed() || queue_.empty(); });

  isClosed_.store(true);

  sendersCond_.notify_all();
  receiversCond_.notify_all();
}

template<typename T, const size_t BufSize>
bool Channel<T, BufSize>::send(T&& value) {
  std::unique_lock<std::mutex> lock(lock_);
  sendersCond_.wait(lock, [this]() { return isClosed() || queue_.size() < (BufSize ? BufSize : 1); });

  if (isClosed())
    return false;

  queue_.emplace_back(std::move(value));
  receiversCond_.notify_all();

  return true;
}

template<typename T, const size_t BufSize>
bool Channel<T, BufSize>::send(const T& value) {
  std::unique_lock<std::mutex> lock(lock_);
  sendersCond_.wait(lock, [this]() { return isClosed() || queue_.size() < (BufSize ? BufSize : 1); });

  if (isClosed())
    return false;

  queue_.emplace_back(value);
  receiversCond_.notify_all();

  return true;
}

template<typename T, const size_t BufSize>
bool Channel<T, BufSize>::receive(T* value) {
  std::unique_lock<std::mutex> lock(lock_);
  receiversCond_.wait(lock, [this]() { return isClosed() || !queue_.empty(); });

  if (isClosed())
    return false;

  *value = std::move(queue_.front());
  queue_.pop_front();
  sendersCond_.notify_all();

  return true;
}

template<typename T, const size_t BufSize>
size_t Channel<T, BufSize>::size() const {
  std::lock_guard<std::mutex> lock(lock_);
  return queue_.size();
}

template<typename T, const size_t BufSize>
bool Channel<T, BufSize>::empty() const {
  return size() == 0;
}

template<typename T, const size_t BufSize>
bool Channel<T, BufSize>::isClosed() const {
  return isClosed_;
}

template<typename T, const size_t BufSize>
Channel<T, BufSize>& Channel<T, BufSize>::operator<<(T&& value) {
  send(std::move(value));
  return *this;
}

template<typename T, const size_t BufSize>
Channel<T, BufSize>& Channel<T, BufSize>::operator>>(T* result) {
  receive(result);
  return *this;
}

} // namespace xzero
