// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RuntimeError.h>

template<typename T>
inline Try<T>::Try(const T& value)
  : success_(true),
    message_() {
  new (storage_) T(value);
}

template<typename T>
inline Try<T>::Try(T&& value)
  : success_(true),
    message_() {
  new (storage_) T(std::move(value));
}

template<typename T>
inline Try<T>::Try(_FailureMessage&& failure)
  : success_(false),
    message_(std::move(failure.message)) {
}

template<typename T>
Try<T>::Try(Try&& other)
    : success_(other.success_),
      message_(std::move(other.message_)) {
  if (success_) {
    new (storage_) T(std::move(*other.get()));
    other.get()->~T();
  }
}


template<typename T>
inline Try<T>::~Try() {
  if (success_) {
    ((T*) &storage_)->~T();
  }
}

template<typename T>
inline Try<T>::operator bool () const noexcept {
  return success_;
}

template<typename T>
inline bool Try<T>::isSuccess() const noexcept {
  return success_;
}

template<typename T>
inline bool Try<T>::isFailure() const noexcept {
  return !success_;
}

template<typename T>
inline const std::string& Try<T>::message() const noexcept {
  return message_;
}

template<typename T>
inline T* Try<T>::get() {
  require();
  return ((T*) &storage_);
}

template<typename T>
inline const T* Try<T>::get() const {
  require();
  return ((T*) &storage_);
}

template<typename T>
inline T* Try<T>::operator->() {
  return get();
}

template<typename T>
inline const T* Try<T>::operator->() const {
  return get();
}

template<typename T>
inline T& Try<T>::operator*() {
  return *get();
}

template<typename T>
inline const T& Try<T>::operator*() const {
  return *get();
}

template<typename T>
inline void Try<T>::require() const {
  using xzero::Status;
  using xzero::RuntimeError;

  if (isFailure())
    RAISE(IllegalStateError);
}

inline _FailureMessage Failure(const std::string& message) {
  return _FailureMessage(std::move(message));
}

template<typename T>
inline Try<T> Success(const T& value) {
  return Try<T>(value);
}

template<typename T>
inline Try<T> Success(T&& value) {
  return Try<T>(std::move(value));
}
