// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/RefCounted.h>
#include <xzero/RefPtr.h>
#include <xzero/Duration.h>
#include <xzero/RuntimeError.h>
#include <xzero/thread/Wakeup.h>
#include <functional>
#include <memory>
#include <mutex>
#include <cstdlib>
#include <system_error>

namespace xzero {

class Scheduler;

template <typename> class Future;
template <typename> class Promise;

template <typename T>
class XZERO_BASE_API PromiseState : public RefCounted {
 public:
  PromiseState();
  ~PromiseState();

 private:
  Wakeup wakeup;
  std::error_code error;
  std::mutex mutex; // FIXPAUL use spinlock
  char value_data[sizeof(T)];
  T* value;
  bool ready;

  std::function<void (std::error_code status)> on_failure;
  std::function<void (const T& value)> on_success;

  friend class Future<T>;
  friend class Promise<T>;
};

template <typename T>
class XZERO_BASE_API Future {
 public:
  Future(RefPtr<PromiseState<T>> promise_state);
  Future(const Future<T>& other);
  Future(Future<T>&& other);
  ~Future();

  Future& operator=(const Future<T>& other);

  bool isReady() const;
  bool isFailure() const;
  bool isSuccess() const;

  template<typename U>
  void onFailure(Promise<U> forward);
  void onFailure(std::function<void (std::error_code ec)> fn);
  void onSuccess(std::function<void (const T& value)> fn);

  void wait() const;
  void wait(const Duration& timeout) const;

  void onReady(std::function<void> fn);
  void onReady(Scheduler* scheduler, std::function<void> fn);

  T& get();
  const T& get() const;
  const T& waitAndGet() const;

  Wakeup* wakeup() const;

 protected:
  RefPtr<PromiseState<T>> state_;
};


template <typename T>
class XZERO_BASE_API Promise {
 public:
  Promise();
  Promise(const Promise<T>& other);
  Promise(Promise<T>&& other);
  ~Promise();

  void success(const T& value) const;
  void success(T&& value) const;
  void failure(const std::exception& e) const;
  void failure(const std::error_code& ec) const;
  void failure(std::errc ec) const;

  Future<T> future() const;
  bool isFulfilled() const;

 protected:
  mutable RefPtr<PromiseState<T>> state_;
};

} // namespace xzero

#include <xzero/thread/Future-impl.h>
