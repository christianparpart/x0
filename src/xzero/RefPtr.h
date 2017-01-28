// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <utility>
#include <atomic>

namespace xzero {

template<typename T>
class XZERO_BASE_API RefPtr {
 public:
  RefPtr() noexcept;
  RefPtr(std::nullptr_t) noexcept;
  RefPtr(T* obj) noexcept;
  RefPtr(RefPtr<T>&& other) noexcept;
  RefPtr(const RefPtr<T>& other) noexcept;
  RefPtr<T>& operator=(RefPtr<T>&& other);
  RefPtr<T>& operator=(const RefPtr<T>& other);
  ~RefPtr();

  operator bool () const noexcept { return !empty(); }

  bool empty() const noexcept;
  size_t refCount() const noexcept;

  T* get() const noexcept;
  T* operator->() const noexcept;
  T& operator*() const noexcept;

  template<typename U>
  U* weak_as() const noexcept;

  template<typename U>
  RefPtr<U> as() const noexcept;

  T* release() noexcept;
  void reset();

  bool operator==(const RefPtr<T>& other) const;
  bool operator!=(const RefPtr<T>& other) const;

  bool operator==(const T* other) const;
  bool operator!=(const T* other) const;

 private:
  T* obj_;
};

template<typename T, typename... Args>
XZERO_BASE_API RefPtr<T> make_ref(Args... args);

// {{{ RefPtr impl
template<typename T>
inline RefPtr<T>::RefPtr() noexcept
    : obj_(nullptr) {
}

template<typename T>
inline RefPtr<T>::RefPtr(std::nullptr_t) noexcept
    : obj_(nullptr) {
}

template<typename T>
inline RefPtr<T>::RefPtr(T* obj) noexcept
    : obj_(obj) {
  if (obj_) {
    obj_->ref();
  }
}

template<typename T>
inline RefPtr<T>::RefPtr(const RefPtr<T>& other) noexcept
    : obj_(other.get()) {
  if (obj_) {
    obj_->ref();
  }
}

template<typename T>
inline RefPtr<T>::RefPtr(RefPtr<T>&& other) noexcept
    : obj_(other.release()) {
}

template<typename T>
inline RefPtr<T>& RefPtr<T>::operator=(RefPtr<T>&& other) {
  if (obj_)
    obj_->unref();

  obj_ = other.release();

  return *this;
}

template<typename T>
inline RefPtr<T>& RefPtr<T>::operator=(const RefPtr<T>& other) {
  if (obj_)
    obj_->unref();

  obj_ = other.obj_;

  if (obj_)
    obj_->ref();

  return *this;
}

template<typename T>
inline RefPtr<T>::~RefPtr() {
  if (obj_) {
    obj_->unref();
  }
}

template<typename T>
bool RefPtr<T>::empty() const noexcept {
  return obj_ == nullptr;
}

template<typename T>
size_t RefPtr<T>::refCount() const noexcept {
  if (obj_) {
    return obj_->refCount();
  } else {
    return 0;
  }
}

template<typename T>
inline T* RefPtr<T>::get() const noexcept {
  return obj_;
}

template<typename T>
inline T* RefPtr<T>::operator->() const noexcept {
  return obj_;
}

template<typename T>
inline T& RefPtr<T>::operator*() const noexcept {
  return *obj_;
}

template<typename T>
template<typename U>
inline U* RefPtr<T>::weak_as() const noexcept {
  return static_cast<U*>(obj_);
}

template<typename T>
template<typename U>
inline RefPtr<U> RefPtr<T>::as() const noexcept {
  return RefPtr<U>(static_cast<U*>(obj_));
}

template<typename T>
inline T* RefPtr<T>::release() noexcept {
  T* t = obj_;
  obj_ = nullptr;
  return t;
}

template<typename T>
inline void RefPtr<T>::reset() {
  if (obj_) {
    obj_->unref();
    obj_ = nullptr;
  }
}

template<typename T>
bool RefPtr<T>::operator==(const RefPtr<T>& other) const {
  return get() == other.get();
}

template<typename T>
bool RefPtr<T>::operator!=(const RefPtr<T>& other) const {
  return get() != other.get();
}

template<typename T>
bool RefPtr<T>::operator==(const T* other) const {
  return get() == other;
}

template<typename T>
bool RefPtr<T>::operator!=(const T* other) const {
  return get() != other;
}
//}}}
// {{{ free functions impl
template<typename T, typename... Args>
inline RefPtr<T> make_ref(Args... args) {
  return RefPtr<T>(new T(std::move(args)...));
}
// }}}

} // namespace xzero
