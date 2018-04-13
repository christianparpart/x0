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
#include <Windows.h>
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

  int readerFd() const noexcept { return fds_[0]; }
  int writerFd() const noexcept { return fds_[1]; }

  void closeReader();
  void closeWriter();

 private:
  int fds_[2];
};

} // namespace xzero