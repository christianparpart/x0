// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <string>
#include <unistd.h>

namespace xzero {

class SystemPipe {
 private:
  enum {
    READER = 0,
    WRITER = 1,
  };

 public:
  SystemPipe(int reader, int writer);
  SystemPipe();
  ~SystemPipe();

  int write(const std::string& msg);

  bool isValid() const noexcept { return fds_[READER] != -1; }
  int readerFd() const noexcept { return fds_[READER]; }
  int writerFd() const noexcept { return fds_[WRITER]; }

  void closeReader();
  void closeWriter();

 private:
  void closeEndPoint(int index);

 private:
  int fds_[2];
};

inline void SystemPipe::closeReader() {
  closeEndPoint(READER);
}

inline void SystemPipe::closeWriter() {
  closeEndPoint(WRITER);
}

inline void SystemPipe::closeEndPoint(int index) {
  if (fds_[index] != -1) {
    ::close(fds_[index]);
  }
}

} // namespace xzero
