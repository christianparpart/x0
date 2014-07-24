// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_LocalStream_hpp
#define sw_x0_LocalStream_hpp 1

#include <base/Api.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

namespace base {

//! \addtogroup base
//@{

/** provides a socket pair (local stream) API.
 */
class BASE_API LocalStream {
  LocalStream& operator=(const LocalStream& v) = delete;
  LocalStream(const LocalStream&) = delete;

 public:
  LocalStream();
  ~LocalStream();

  int local();
  int remote();

  void closeAll();
  void closeLocal();
  void closeRemote();

 private:
  int pfd_[2];
};

// {{{ inlines
inline LocalStream::LocalStream() {
  // TODO consider using pipe2() instead of socketpair() - what's the diff?
  int opts = 0;

#if defined(SOCK_NONBLOCK)
  opts |= SOCK_NONBLOCK;
#endif

#if defined(SOCK_CLOEXEC)
  opts |= SOCK_CLOEXEC;
#endif

  if (socketpair(AF_UNIX, SOCK_STREAM | opts, 0, pfd_) < 0) {
    pfd_[0] = pfd_[1] = -1;
  } else {
#if !defined(SOCK_NONBLOCK)
    fcntl(pfd_[0], F_SETFD, fcntl(pfd_[0], F_GETFL) | O_NONBLOCK);
    fcntl(pfd_[1], F_SETFD, fcntl(pfd_[1], F_GETFL) | O_NONBLOCK);
#endif

#if !defined(SOCK_CLOEXEC)
    fcntl(pfd_[0], F_SETFD, fcntl(pfd_[0], F_GETFL) | FD_CLOEXEC);
    fcntl(pfd_[1], F_SETFD, fcntl(pfd_[1], F_GETFL) | FD_CLOEXEC);
#endif
  }
}

inline LocalStream::~LocalStream() { closeAll(); }

inline int LocalStream::local() { return pfd_[0]; }

inline int LocalStream::remote() { return pfd_[1]; }

inline void LocalStream::closeAll() {
  if (pfd_[0] != -1) {
    ::close(pfd_[0]);
    pfd_[0] = -1;
  }

  if (pfd_[1] != -1) {
    ::close(pfd_[1]);
    pfd_[1] = -1;
  }
}

inline void LocalStream::closeLocal() {
  if (pfd_[0] != -1) {
    ::close(pfd_[0]);
    pfd_[0] = -1;
  }
}

inline void LocalStream::closeRemote() {
  if (pfd_[1] != -1) {
    ::close(pfd_[1]);
    pfd_[1] = -1;
  }
}
// }}}

//@}

}  // namespace base

#endif
