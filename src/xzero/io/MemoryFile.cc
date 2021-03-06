// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Buffer.h>
#include <xzero/RuntimeError.h>
#include <xzero/StringUtil.h>
#include <xzero/hash/FNV.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileHandle.h>
#include <xzero/io/MemoryFile.h>
#include <xzero/sysconfig.h>

#include <cerrno>
#include <ctime>
#include <fstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#if defined(XZERO_OS_UNIX)
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#endif

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

namespace xzero {

MemoryFile::MemoryFile()
    : File("", ""),
      mtime_(0),
      inode_(0),
      size_(0),
      etag_(),
      fspath_(),
      fd_() {
}

MemoryFile::MemoryFile(
    const std::string& path,
    const std::string& mimetype,
    const BufferRef& data,
    UnixTime mtime)
    : File(path, mimetype),
      mtime_(mtime),
      inode_(0),
      size_(data.size()),
      etag_(std::to_string(data.hash())),
      fspath_(),
      fd_() {
  fd_ = FileUtil::createTempFileAt(FileUtil::tempDirectory(), &fspath_);
  if (fd_ < 0)
    RAISE_ERRNO(errno);

  ssize_t n = fd_.write(data.data(), data.size());
  if (n < 0)
    RAISE_ERRNO(errno);

  if (static_cast<size_t>(n) != data.size())
    RAISE_ERRNO(EIO);
}

MemoryFile::~MemoryFile() {
}

const std::string& MemoryFile::etag() const {
  return etag_;
}

size_t MemoryFile::size() const noexcept {
  return size_;
}

UnixTime MemoryFile::mtime() const noexcept {
  return mtime_;
}

size_t MemoryFile::inode() const noexcept {
  return inode_;
}

bool MemoryFile::isRegular() const noexcept {
  return true;
}

bool MemoryFile::isDirectory() const noexcept {
  return false;
}

bool MemoryFile::isExecutable() const noexcept {
  return false;
}

FileHandle MemoryFile::createPosixChannel(FileOpenFlags oflags) {
#if defined(XZERO_OS_WINDOWS)
  RAISE_NOT_IMPLEMENTED();
#else
  if (fd_.isClosed()) {
    errno = ENOENT;
    return FileHandle{};
  }

  return fd_.dup(oflags);
#endif
}

} // namespace xzero
