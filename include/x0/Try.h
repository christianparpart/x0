// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <memory>

#include <x0/Api.h>

namespace x0 {

struct Error {
  explicit Error(const char* msg) : msg_(msg) {}

  const char* msg_;
};

struct Errno : public Error {
  Errno() : Error(strerror(errno)) {}
};

/**
 * Provides a typed value wrapper which also encodes a possible error status
 * that avoids exception handling.
 */
template <typename T>
class X0_API Try {
 public:
  Try() : value_(), errorMessage_(nullptr) {}
  Try(const Error& err) : value_(), errorMessage_(err.msg_) {}
  Try(const T& value) : value_(value), errorMessage_(nullptr) {}

  Try(const Try<T>& other) = default;
  Try<T>& operator=(const Try<T>& other) = default;

  Try(T&& value) : value_(std::move(value)), errorMessage_(nullptr) {}
  Try<T>& operator=(Try<T>&& other) {
    value_ = std::move(other.value_);
    errorMessage_ = other.errorMessage_;
    other.errorMessage_ = nullptr;
    return *this;
  }

  bool isOkay() const { return errorMessage_ == nullptr; }
  bool isError() const { return errorMessage_ != nullptr; }
  operator bool() const { return !isError(); }

  const char* errorMessage() const {
    requireError();
    return errorMessage_;
  }

  T& get() {
    require();
    return value_;
  }
  const T& get() const {
    require();
    return value_;
  }

  T& operator*() {
    require();
    return value_;
  }
  const T& operator*() const {
    require();
    return value_;
  }

  T* operator->() {
    require();
    return &value_;
  }
  const T* operator->() const {
    require();
    return &value_;
  }

  void require() const {
    if (isError()) {
      fprintf(stderr, "Unchecked access to a Try<> instance.\n");
      abort();
    }
  }

  void requireError() const {
    if (!isError()) {
      fprintf(stderr, "Unchecked access to a Try<> instance.\n");
      abort();
    }
    assert(isError());
  }

  void clear() { errorMessage_ = nullptr; }

  template <typename U>
  Try<T> onOkay(U block) const {
    if (isOkay()) {
      block(get());
    }
    return *this;
  }

  template <typename U>
  Try<T> onError(U block) const {
    if (isError()) {
      block(errorMessage());
    }
    return *this;
  }

 private:
  T value_;
  const char* errorMessage_;
};

template <typename T>
bool operator==(const Try<T>& a, const Try<T>& b) {
  if (a.isError() && b.isError())
    return strcmp(a.errorMessage(), b.errorMessage()) == 0;

  if (a.isOkay() && b.isOkay()) return a.get() == b.get();

  return false;
}

template <typename T>
bool operator!=(const Try<T>& a, const Try<T>& b) {
  return !(a == b);
}

template <typename T>
Try<T> Okay(const T& value) {
  return Try<T>(value);
}

}  // namespace x0
