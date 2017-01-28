// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/FileOutputStream.h>
#include <xzero/io/FileUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace xzero {

FileOutputStream::FileOutputStream(const std::string& path, File::OpenFlags flags, int mode)
    : handle_(::open(path.c_str(), File::to_posix(File::Write | flags), mode)),
      closeOnDestroy_(true) {
  if (handle_ < 0) {
    RAISE_ERRNO(errno);
  }
}

FileOutputStream::~FileOutputStream() {
  if (closeOnDestroy_) {
    FileUtil::close(handle_);
  }
}

int FileOutputStream::write(const char* buf, size_t size) {
  ssize_t n = ::write(handle_, buf, size);
  if (n < 0) {
    RAISE_ERRNO(errno);
  }
  return n;
}

} // namespace xzero
