// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

template<typename T>
inline Result<T>::Result(const value_type& value)
  : value_(value) {}

template<typename T>
inline Result<T>::Result(value_type&& value)
  : value_(std::move(value)) {}

template<typename T>
inline Result<T>::Result(const std::error_code& ec)
  : error_(ec) {
  if (!error_) {
    throw std::invalid_argument("Result<> received an error_code that does not contain an error.");
  }
}

template<typename T>
inline Result<T>::operator bool () const noexcept {
  return bool{ value_ };
}

template<typename T>
inline bool Result<T>::isSuccess() const noexcept {
  return bool{ value_ };
}

template<typename T>
inline bool Result<T>::isFailure() const noexcept {
  return !value_;
}

template<typename T>
inline const std::error_code& Result<T>::error() const noexcept {
  return error_;
}

template<typename T>
inline typename Result<T>::reference_type Result<T>::value() {
  require();
  return *value_;
}

template<typename T>
inline const typename Result<T>::reference_type Result<T>::value() const {
  require();
  return *value_;
}

template<typename T>
inline typename Result<T>::pointer_type Result<T>::get() {
  return &value();
}

template<typename T>
inline const typename Result<T>::pointer_type Result<T>::get() const {
  return &value();
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
  if (isFailure()) {
    throw ResultBadAccess();
  }
}

template<typename T>
inline Result<T> Success(T&& value) {
  return Result<T>(std::forward<T>(value));
}
