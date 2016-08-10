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

/**
 * @brief Try<T> gives you the opportunity to either return some value or an error.
 *
 * @code{.cpp}
 *  void main() {
 *    Try<int> uid = getUserID();
 *    if (uid.isFailure()) {
 *      fail("Could not retrieve userID. $0", uid.failure());
 *    } else {
 *      info("User ID is $0", *uid);
 *    }
 *  }
 *  Try<int> getUserID() {
 *    if (getuid() == 0)
 *      return Failure("Sorry, I'm not talking to root.");
 *    else
 *      return Success(getuid());
 *  }
 * @endcode
 */
template<typename T>
class Try {
 public:
  Try(const T& value);
  Try(T&& value);
  Try(_FailureMessage&& message);
  Try(Try&& other);
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

/**
 * Generates Try<T> object that represents a successful item of value @p value.
 */
template<typename T>
Try<T> Success(const T& value);

/**
 * Generates Try<T> object that represents a successful item of value @p value.
 */
template<typename T>
Try<T> Success(T&& value);

/**
 * Generates Try<T> helper object that represents a failure with given @p message.
 */
_FailureMessage Failure(const std::string& message);

#include <xzero/Try-inl.h>
