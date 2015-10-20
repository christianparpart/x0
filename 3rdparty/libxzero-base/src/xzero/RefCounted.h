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
#include <atomic>

namespace xzero {

class XZERO_BASE_API RefCounted {
 public:
  RefCounted() noexcept;
  virtual ~RefCounted();

  void ref() noexcept;
  bool unref();

  XZERO_DEPRECATED void incRef() { ref(); }
  XZERO_DEPRECATED void decRef() { unref(); }

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
