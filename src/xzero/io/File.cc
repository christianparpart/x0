// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/File.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

#include <ctime>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#if 1
#define TRACE(level, msg...) logTrace("File", msg)
#else
#define TRACE(level, msg...) do {} while (0)
#endif

namespace xzero {

File::File(const std::string& path, const std::string& mimetype)
    : path_(path),
      mimetype_(mimetype),
      errno_(0),
      lastModified_() {
  TRACE(2, "($0).ctor", path_);
}

File::~File() {
  TRACE(2, "($0).dtor", path_);
}

std::string File::filename() const {
  size_t n = path_.rfind('/');
  return n != std::string::npos ? path_.substr(n + 1) : path_;
}

const std::string& File::lastModified() const {
  // build Last-Modified response header value on-demand
  if (lastModified_.empty()) {
    time_t modificationTime = mtime();
    struct tm tm;
    if (gmtime_r(&modificationTime, &tm)) {
      char buf[256];

      if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", &tm) != 0) {
        lastModified_ = buf;
      }
    }
  }

  return lastModified_;
}

int File::to_posix(OpenFlags oflags) {
  int flags = 0;

#if defined(O_LARGEFILE)
  flags |= O_LARGEFILE;
#endif

  if ((oflags & ReadWrite) == ReadWrite)
    flags |= O_RDWR;
  else if (oflags & Read)
    flags |= O_RDONLY;
  else if (oflags & Write)
    flags |= O_WRONLY;

  if (oflags & Create)
    flags |= O_CREAT;

  if (oflags & CreateNew)
    flags |= O_CREAT | O_EXCL;

  if (oflags & Truncate)
    flags |= O_TRUNC;

  if (oflags & Append)
    flags |= O_APPEND;

#if defined(O_CLOEXEC)
  if (!(oflags & Share))
    flags |= O_CLOEXEC;
#endif

#if defined(O_NONBLOCK)
  if (oflags & NonBlocking)
    flags |= O_NONBLOCK;
#endif

#if defined(O_TMPFILE)
  if (oflags & TempFile)
    flags |= O_TMPFILE;
#endif

  return flags;
}

} // namespace xzero
