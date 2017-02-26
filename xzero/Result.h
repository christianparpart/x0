// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
// Inspired by Mesos' libprocess Result<>

#include <string>
#include <system_error>

/**
 * @brief Result<T> gives you the opportunity to either return some value or an error.
 *
 * @code{.cpp}
 *  void main() {
 *    Result<int> uid = getUserID();
 *    if (uid.isFailure()) {
 *      fail("Could not retrieve userID. $0", uid.failureMessage());
 *    } else {
 *      info("User ID is $0", *uid);
 *    }
 *  }
 *  Result<int> getUserID() {
 *    if (getuid() == 0)
 *      return Failure("Sorry, I'm not talking to root.");
 *    else
 *      return Success(getuid());
 *  }
 * @endcode
 */
template<typename T>
class Result {
 public:
  Result(const T& value);
  Result(T&& value);
  Result(const std::error_code& message);
  Result(Result&& other);
  ~Result();

  operator bool () const noexcept;
  bool isSuccess() const noexcept;
  bool isFailure() const noexcept;
  const std::string failureMessage() const;
  const std::error_code& error() const noexcept;

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
  std::error_code error_;
};

/**
 * Generates Result<T> object that represents a successful item of value @p value.
 */
template<typename T>
Result<T> Success(const T& value);

/**
 * Generates Result<T> object that represents a successful item of value @p value.
 */
template<typename T>
Result<T> Success(T&& value);

#include <xzero/Result-inl.h>
