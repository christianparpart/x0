// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <cassert>

namespace xzero {

template <typename T>
PromiseState<T>::PromiseState() :
    error(),
    value(nullptr),
    ready(false),
    on_failure(nullptr),
    on_success(nullptr) {
}

template <typename T>
PromiseState<T>::~PromiseState() {
  assert(ready == true);

  if (value != nullptr) {
    value->~T();
  }
}

template <typename T>
Future<T>::Future(RefPtr<PromiseState<T>> state) : state_(state) {}

template <typename T>
Future<T>::Future(const Future<T>& other) : state_(other.state_) {}

template <typename T>
Future<T>::Future(Future<T>&& other) : state_(std::move(other.state_)) {}

template <typename T>
Future<T>::~Future() {}

template <typename T>
Future<T>& Future<T>::operator=(const Future<T>& other) {
  state_ = other.state_;
}

template <typename T>
void Future<T>::wait() const {
  state_->wakeup.waitForFirstWakeup();
}

template<typename T>
template<typename U>
void Future<T>::onFailure(Promise<U> forward) {
  onFailure([this, forward](const std::error_code& ec) {
    forward.failure(ec);
  });
}

template <typename T>
void Future<T>::onFailure(std::function<void(std::error_code)> fn) {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    state_->on_failure = fn;
  } else if (state_->error) {
    fn(state_->error);
  }
}

template <typename T>
void Future<T>::onSuccess(std::function<void (const T& value)> fn) {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    state_->on_success = fn;
  } else if (!state_->error) {
    fn(*(state_->value));
  }
}

template <typename T>
bool Future<T>::isReady() const {
  return state_->ready;
}

template <typename T>
bool Future<T>::isSuccess() const {
  return isReady() && !state_->error;
}

template <typename T>
bool Future<T>::isFailure() const {
  return isReady() && state_->error;
}

template <typename T>
T& Future<T>::get() {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    RAISE(FutureError, "get() called on pending future");
  }

  if (state_->error)
    RAISE_ERROR(state_->error);

  return *state_->value;
}

template <typename T>
const T& Future<T>::get() const {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    RAISE(FutureError, "get() called on pending future");
  }

  if (state_->error)
    RAISE_ERROR(state_->error);

  return *state_->value;
}

template <typename T>
const std::error_code& Future<T>::error() const {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    RAISE(FutureError, "error() called on pending future");
  }

  if (!state_->error)
    RAISE(InternalError, "erorr() called but no error received");

  return state_->error;
}

template <typename T>
Wakeup* Future<T>::wakeup() const {
  return &state_->wakeup;
}

template <typename T>
const T& Future<T>::waitAndGet() const {
  wait();
  return get();
}

template <typename T>
Result<T> Future<T>::waitAndGetResult() const {
  wait();
  if (isFailure()) {
    return Result<T>(state_->error);
  } else {
    return Result<T>(get());
  }
}

template <typename T>
Promise<T>::Promise() : state_(new PromiseState<T>()) {}

template <typename T>
Promise<T>::Promise(const Promise<T>& other) : state_(other.state_) {}

template <typename T>
Promise<T>::Promise(Promise<T>&& other) : state_(std::move(other.state_)) {}

template <typename T>
Promise<T>::~Promise() {}

template <typename T>
Promise<T>& Promise<T>::operator=(const Promise<T>& other) {
  state_ = other.state_;
  return *this;
}

template <typename T>
Promise<T>& Promise<T>::operator=(Promise&& other) {
  state_ = std::move(other.state_);
  return *this;
}

template <typename T>
Future<T> Promise<T>::future() const {
  return Future<T>(state_);
}

template <typename T>
void Promise<T>::failure(std::errc ec) const {
  failure(std::make_error_code(ec));
}

template <typename T>
void Promise<T>::failure(const std::error_code& error) const {
  std::unique_lock<std::mutex> lk(state_->mutex);
  if (state_->ready) {
    RAISE(FutureError, "promise was already fulfilled");
  }

  state_->error = error;
  state_->ready = true;
  lk.unlock();

  state_->wakeup.wakeup();

  if (state_->on_failure) {
    state_->on_failure(state_->error);
  }
}

template <typename T>
void Promise<T>::success(const T& value) const {
  std::unique_lock<std::mutex> lk(state_->mutex);
  if (state_->ready) {
    RAISE(FutureError, "promise was already fulfilled");
  }

  state_->value = new (state_->value_data) T(value);
  state_->ready = true;
  lk.unlock();

  state_->wakeup.wakeup();

  if (state_->on_success) {
    state_->on_success(*(state_->value));
  }
}

template <typename T>
void Promise<T>::success(T&& value) const {
  std::unique_lock<std::mutex> lk(state_->mutex);
  if (state_->ready) {
    RAISE(FutureError, "promise was already fulfilled");
  }

  state_->value = new (state_->value_data) T(std::move(value));
  state_->ready = true;
  lk.unlock();

  state_->wakeup.wakeup();

  if (state_->on_success) {
    state_->on_success(*(state_->value));
  }
}

template <typename T>
bool Promise<T>::isFulfilled() const {
  std::unique_lock<std::mutex> lk(state_->mutex);
  return state_->ready;
}

// {{{ PromiseState<void>
inline PromiseState<void>::PromiseState() :
    error(),
    ready(false),
    on_failure(nullptr),
    on_success(nullptr) {
}

inline PromiseState<void>::~PromiseState() {
  assert(ready == true);
}
// }}}
// {{{ Future<void>
inline Future<void>::Future(RefPtr<PromiseState<void>> state) : state_(state) {}

inline Future<void>::Future(const Future<void>& other) : state_(other.state_) {}

inline Future<void>::Future(Future<void>&& other) : state_(std::move(other.state_)) {}

inline Future<void>::~Future() {}

inline Future<void>& Future<void>::operator=(const Future<void>& other) {
  state_ = other.state_;
  return *this;
}

inline void Future<void>::wait() const {
  state_->wakeup.waitForFirstWakeup();
}

template<typename U>
inline void Future<void>::onFailure(Promise<U> forward) {
  onFailure([this, forward](const std::error_code& ec) {
    forward.failure(ec);
  });
}

inline void Future<void>::onFailure(std::function<void(std::error_code)> fn) {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    state_->on_failure = fn;
  } else if (state_->error) {
    fn(state_->error);
  }
}

inline void Future<void>::onSuccess(std::function<void ()> fn) {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    state_->on_success = fn;
  } else if (!state_->error) {
    fn();
  }
}

inline bool Future<void>::isReady() const {
  return state_->ready;
}

inline bool Future<void>::isSuccess() const {
  return isReady() && !state_->error;
}

inline bool Future<void>::isFailure() const {
  return isReady() && state_->error;
}

inline const std::error_code& Future<void>::error() const {
  std::unique_lock<std::mutex> lk(state_->mutex);

  if (!state_->ready) {
    RAISE(FutureError, "error() called on pending future");
  }

  if (!state_->error)
    RAISE(InternalError, "erorr() called but no error received");

  return state_->error;
}

inline Wakeup* Future<void>::wakeup() const {
  return &state_->wakeup;
}
// }}}
// {{{ Promise<void>
inline Promise<void>::Promise() : state_(new PromiseState<void>()) {}

inline Promise<void>::Promise(const Promise<void>& other) : state_(other.state_) {}

inline Promise<void>::Promise(Promise<void>&& other) : state_(std::move(other.state_)) {}

inline Promise<void>::~Promise() {}

inline Promise<void>& Promise<void>::operator=(const Promise<void>& other) {
  state_ = other.state_;
  return *this;
}

inline Promise<void>& Promise<void>::operator=(Promise&& other) {
  state_ = std::move(other.state_);
  return *this;
}

inline Future<void> Promise<void>::future() const {
  return Future<void>(state_);
}

inline void Promise<void>::failure(std::errc ec) const {
  failure(std::make_error_code(ec));
}

inline void Promise<void>::failure(const std::error_code& error) const {
  std::unique_lock<std::mutex> lk(state_->mutex);
  if (state_->ready) {
    RAISE(FutureError, "promise was already fulfilled");
  }

  state_->error = error;
  state_->ready = true;
  lk.unlock();

  state_->wakeup.wakeup();

  if (state_->on_failure) {
    state_->on_failure(state_->error);
  }
}

inline void Promise<void>::success() const {
  std::unique_lock<std::mutex> lk(state_->mutex);
  if (state_->ready) {
    RAISE(FutureError, "promise was already fulfilled");
  }

  state_->ready = true;
  lk.unlock();

  state_->wakeup.wakeup();

  if (state_->on_success) {
    state_->on_success();
  }
}

inline bool Promise<void>::isFulfilled() const {
  std::unique_lock<std::mutex> lk(state_->mutex);
  return state_->ready;
}
// }}}

} // namespace xzero
