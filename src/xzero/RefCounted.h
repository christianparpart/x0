// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <atomic>

namespace xzero {

class XZERO_BASE_API RefCounted {
 public:
  RefCounted() noexcept;
  virtual ~RefCounted();

  void ref() noexcept;
  bool unref();

  void incRef() { ref(); }
  void decRef() { unref(); }

  unsigned refCount() const noexcept;

 private:
  std::atomic<unsigned> refCount_;
};

// {{{ RefCounted impl
inline RefCounted::RefCounted() noexcept
    : refCount_(0) {
}

inline RefCounted::~RefCounted() {
}

inline unsigned RefCounted::refCount() const noexcept {
  return refCount_.load();
}

inline void RefCounted::ref() noexcept {
  refCount_++;
}

inline bool RefCounted::unref() {
  if (std::atomic_fetch_sub_explicit(&refCount_, 1u,
                                     std::memory_order_release) == 1) {
    std::atomic_thread_fence(std::memory_order_acquire);
    delete this;
    return true;
  }

  return false;
}
// }}}

} // namespace xzero
