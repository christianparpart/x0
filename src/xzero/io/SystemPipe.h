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

#if defined(XZERO_OS_WIN32)
#include <windef.h>
#endif

namespace xzero {

class SystemPipe {
 public:
  SystemPipe();
  ~SystemPipe();

  void setNonBlocking(bool enable);

  int write(const std::string& msg);
  int write(const void* buf, size_t count);
  void consume();

#if defined(XZERO_OS_WIN32)
  HANDLE readerFd() const noexcept { return reader_; }
  HANDLE writerFd() const noexcept { return writer_; }
#else
  int readerFd() const noexcept { return fds_[0]; }
  int writerFd() const noexcept { return fds_[1]; }
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

} // namespace xzero
