// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RuntimeError.h>
#include <xzero/Status.h>
#include <xzero/StringUtil.h>

template<typename T>
inline Result<T>::Result(const value_type& value)
  : success_(true),
    error_() {
  new (storage_) value_type(value);
}

template<typename T>
inline Result<T>::Result(value_type&& value)
  : success_(true),
    error_() {
  new (storage_) value_type(std::move(value));
}

template<typename T>
inline Result<T>::Result(const std::error_code& ec)
  : success_(false),
    error_(ec) {
  if (!error_) {
    using xzero::Status;
    using xzero::RuntimeError;

    // "received an error_code in Result, but there is no error."
    RAISE(InternalError);
  }
}

template<typename T>
Result<T>::Result(Result&& other)
    : success_(other.success_),
      error_(std::move(other.error_)) {
  if (success_) {
    new (storage_) value_type(std::move(*other.get()));
    other.get()->~value_type();
  }
}


template<typename T>
inline Result<T>::~Result() {
  if (success_) {
    ((value_type*) &storage_)->~value_type();
  }
}

template<typename T>
inline Result<T>::operator bool () const noexcept {
  return success_;
}

template<typename T>
inline bool Result<T>::isSuccess() const noexcept {
  return success_;
}

template<typename T>
inline bool Result<T>::isFailure() const noexcept {
  return !success_;
}

template<typename T>
inline const std::string Result<T>::failureMessage() const {
  return error_.message();
}

template<typename T>
inline const std::error_code& Result<T>::error() const noexcept {
  return error_;
}

template<typename T>
inline typename Result<T>::pointer_type Result<T>::get() {
  require();
  return ((pointer_type) &storage_);
}

template<typename T>
inline const typename Result<T>::pointer_type Result<T>::get() const {
  require();
  return ((pointer_type) &storage_);
}

template<typename T>
inline typename Result<T>::pointer_type Result<T>::operator->() {
  return get();
}

template<typename T>
inline const typename Result<T>::pointer_type Result<T>::operator->() const {
  return get();
}

template<typename T>
inline typename Result<T>::value_type& Result<T>::operator*() {
  return *get();
}

template<typename T>
inline const typename Result<T>::value_type& Result<T>::operator*() const {
  return *get();
}

template<typename T>
inline void Result<T>::require() const {
  using xzero::Status;
  using xzero::RuntimeError;

  if (isFailure())
    RAISE(IllegalStateError);
}

template<typename T>
inline Result<T> Success(const T& value) {
  return Result<T>(value);
}

template<typename T>
inline Result<T> Success(T&& value) {
  return Result<T>(std::move(value));
}
