// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/RuntimeError.h>

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <memory>

namespace xzero {

struct None {};

/**
 * Option<> type class.
 */
template <typename T>
class XZERO_BASE_API Option {
 public:
  Option();
  Option(const None&);
  Option(const T& value);
  Option(const Option<T>& other);
  Option(T&& value);
  ~Option();

  Option<T>& operator=(const Option<T>& other);
  Option<T>& operator=(Option<T>&& other);

  bool isSome() const noexcept;
  bool isNone() const noexcept;
  bool isEmpty() const noexcept;
  operator bool() const noexcept;

  void reset();
  void clear();

  void set(const Option<T>& other);
  void set(Option<T>&& other);
  void set(const T& other);
  void set(T&& other);

  T& get();
  const T& get() const;

  const T& getOrElse(const T& alt) const;

  T& operator*();
  const T& operator*() const;

  T* operator->();
  const T* operator->() const;

  void require() const;
  void requireNone() const;

  template <typename U> Option<T> onSome(U block) const;
  template <typename U> Option<T> onNone(U block) const;

 private:
  unsigned char storage_[sizeof(T)];
  bool valid_;
};

template <typename T> bool operator==(const Option<T>& a, const Option<T>& b);
template <typename T> bool operator==(const Option<T>& a, const None& b);
template <typename T> bool operator!=(const Option<T>& a, const Option<T>& b);
template <typename T> bool operator!=(const Option<T>& a, const None& b);

template <typename T> Option<T> Some(const T& value);

}  // namespace xzero

#include <xzero/Option-inl.h>
