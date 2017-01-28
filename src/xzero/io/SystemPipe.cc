// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/SystemPipe.h>
#include <xzero/RuntimeError.h>
#include <unistd.h>

namespace xzero {

SystemPipe::SystemPipe(int reader, int writer) {
  fds_[READER] = reader;
  fds_[WRITER] = writer;
}

SystemPipe::SystemPipe() : SystemPipe(-1, -1) {
  if (pipe(fds_) < 0) {
    RAISE_ERRNO(errno);
  }
}

SystemPipe::~SystemPipe() {
  closeReader();
  closeWriter();
}

int SystemPipe::write(const std::string& msg) {
  return ::write(writerFd(), msg.data(), msg.size());
}

} // namespace xzero
