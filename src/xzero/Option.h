// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
