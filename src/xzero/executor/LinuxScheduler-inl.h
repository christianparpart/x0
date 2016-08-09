// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

namespace xzero {

inline LinuxScheduler::Watcher::Watcher()
        : Watcher(-1, Mode::READABLE, nullptr, MonotonicTime::Zero, nullptr) {
}

inline LinuxScheduler::Watcher::Watcher(int _fd, Mode _mode, Task _onIO,
                                 MonotonicTime _timeout, Task _onTimeout)
  : fd(_fd),
    mode(_mode),
    timeout(_timeout),
    onTimeout(_onTimeout),
    prev(nullptr),
    next(nullptr) {
  // Manually ref because we're not holding it in a
  // RefPtr<Watcher> vector in PosixScheduler.
  // - Though, no need to manually unref() either.
  ref();
}

inline void LinuxScheduler::Watcher::reset(int fd, Mode mode, Task onIO,
                                           MonotonicTime timeout, Task onTimeout) {
  this->fd = fd;
  this->mode = mode;
  this->onIO = onIO;
  this->timeout = timeout;
  this->onTimeout = onTimeout;
  this->prev = nullptr;
  this->next = nullptr;
}

} // namespace xzero
