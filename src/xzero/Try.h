// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
// Inspired by Mesos' libprocess Try<>

struct _FailureMessage {
  explicit _FailureMessage(const std::string& msg) : message(msg) {}
  explicit _FailureMessage(std::string&& msg) : message(std::move(msg)) {}
  std::string message;
};

template<typename T>
class Try {
 public:
  Try(const T& value);
  Try(T&& value);
  Try(_FailureMessage&& message);
  ~Try();

  operator bool () const noexcept;
  bool isSuccess() const noexcept;
  bool isFailure() const noexcept;
  const std::string& message() const noexcept;

  T* get();
  const T* get() const;

  T* operator->();
  const T* operator->() const;

  T& operator*();
  const T& operator*() const;

  void require() const;

 private:
  bool success_;
  unsigned char storage_[sizeof(T)];
  std::string message_;
};

template<typename T>
Try<T> Success(const T& value);

template<typename T>
Try<T> Success(T&& value);

_FailureMessage Failure(const std::string& message);

#include <xzero/Try-inl.h>
