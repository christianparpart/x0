// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

namespace xzero {

template<typename T>
inline Option<T>::Option() : valid_(false) {}

template<typename T>
inline Option<T>::Option(const None&) : valid_(false) {}

template<typename T>
inline Option<T>::Option(const T& value) : valid_(false) { set(value); }

template<typename T>
inline Option<T>::Option(const Option<T>& other) : valid_(false) { set(other); }

template<typename T>
inline Option<T>::Option(T&& value) : valid_(false) { set(std::move(value)); }

template<typename T>
inline Option<T>::~Option() { reset(); }

template<typename T>
inline Option<T>& Option<T>::operator=(const Option<T>& other) {
  set(other);
  return *this;
}

template<typename T>
inline Option<T>& Option<T>::operator=(Option<T>&& other) {
  set(std::move(other));
  return *this;
}

template<typename T>
inline bool Option<T>::isSome() const noexcept { return valid_; }

template<typename T>
inline bool Option<T>::isNone() const noexcept { return !valid_; }

template<typename T>
inline bool Option<T>::isEmpty() const noexcept { return !valid_; }

template<typename T>
inline Option<T>::operator bool() const noexcept { return isSome(); }

template<typename T>
inline void Option<T>::reset() {
  if (valid_) {
    ((T*) &storage_)->~T();
    valid_ = false;
  }
}

template<typename T>
inline void Option<T>::clear() {
  reset();
}

template<typename T>
inline void Option<T>::set(const Option<T>& other) {
  if (other.isSome()) {
    set(other.get());
  } else {
    reset();
  }
}

template<typename T>
inline void Option<T>::set(Option<T>&& other) {
  if (other.isSome()) {
    set(std::move(other.get()));
    other.valid_ = false;
  } else {
    reset();
  }
}

template<typename T>
inline void Option<T>::set(const T& other) {
  reset();
  new (storage_) T(other);
  valid_ = true;
}

template<typename T>
inline void Option<T>::set(T&& other) {
  reset();
  new (storage_) T(std::move(other));
  valid_ = true;
}

template<typename T>
inline T& Option<T>::get() {
  require();
  return *((T*) &storage_);
}

template<typename T>
inline const T& Option<T>::get() const {
  require();
  return *((T*) &storage_);
}

template<typename T>
const T& Option<T>::getOrElse(const T& alt) const {
  if (isSome())
    return *((T*) &storage_);
  else
    return alt;
}

template<typename T>
inline T& Option<T>::operator*() {
  return get();
}

template<typename T>
inline const T& Option<T>::operator*() const {
  return get();
}

template<typename T>
inline T* Option<T>::operator->() {
  require();
  return ((T*) &storage_);
}

template<typename T>
inline const T* Option<T>::operator->() const {
  require();
  return ((T*) &storage_);
}

template<typename T>
inline void Option<T>::require() const {
  if (isNone())
    RAISE(OptionUncheckedAccessToInstance);
}

template<typename T>
inline void Option<T>::requireNone() const {
  if (!isNone())
    RAISE(OptionUncheckedAccessToInstance);
}

template<typename T>
template <typename U>
inline Option<T> Option<T>::onSome(U block) const {
  if (isSome())
    block(get());

  return *this;
}

template<typename T>
template <typename U>
inline Option<T> Option<T>::onNone(U block) const {
  if (isNone())
    block();

  return *this;
}

// --------------------------------------------------------------------------

template <typename T>
inline bool operator==(const Option<T>& a, const Option<T>& b) {
  if (a.isSome() && b.isSome())
    return a.get() == b.get();

  if (a.isNone() && b.isNone())
    return true;

  return false;
}

template <typename T>
inline bool operator==(const Option<T>& a, const None& /*b*/) {
  return a.isNone();
}

template <typename T>
inline bool operator!=(const Option<T>& a, const Option<T>& b) {
  return !(a == b);
}

template <typename T>
inline bool operator!=(const Option<T>& a, const None& b) {
  return !(a == b);
}

template <typename T>
inline Option<T> Some(const T& value) {
  return Option<T>(value);
}

} // namespace xzero
