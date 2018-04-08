// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <string>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(HAVE_WINDOWS_H)
#include <Windows.h>
#endif

namespace xzero {

class SystemPipe {
 private:
  enum {
    READER = 0,
    WRITER = 1,
  };

 public:
  SystemPipe();
  ~SystemPipe();

  int write(const std::string& msg);

#if defined(XZERO_OS_WIN32)
  HANDLE readerFd() const noexcept { return reader_; }
  HANDLE writerFd() const noexcept { return writer_; }
#else
  int readerFd() const noexcept { return fds_[READER]; }
  int writerFd() const noexcept { return fds_[WRITER]; }
#endif

  void closeReader();
  void closeWriter();

 private:
#if defined(XZERO_OS_WIN32)
   HANDLE writer_;
   HANDLE reader_;
#else
  int fds_[2];
#endif
};

inline void SystemPipe::closeReader() {
#if defined(XZERO_OS_WIN32)
  CloseHandle(reader_);
#else
  if (fds_[READER] != -1) {
    ::close(fds_[READER]);
    fds_[READER] = -1;
  }
#endif
}

inline void SystemPipe::closeWriter() {
#if defined(XZERO_OS_WIN32)
  CloseHandle(writer_);
#else
  if (fds_[WRITER] != -1) {
    ::close(fds_[WRITER]);
    fds_[WRITER] = -1;
  }
#endif
}

} // namespace xzero
