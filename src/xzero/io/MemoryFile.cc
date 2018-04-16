// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
#include <xzero/io/FileUtil.h>
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

namespace xzero {

// Since OS/X doesn't support SHM in a way Linux does, we need to work around it.
#if XZERO_OS_DARWIN
#  define XZERO_MEMORYFILE_USE_TMPFILE
#else
// Use TMPFILE here, too (for now), because create*Channel would fail otherwise.
// So let's create a posix_filebuf
//#  define XZERO_MEMORYFILE_USE_SHM
#  define XZERO_MEMORYFILE_USE_TMPFILE
#endif

MemoryFile::MemoryFile()
    : File("", ""),
      mtime_(0),
      inode_(0),
      size_(0),
      etag_(),
      fspath_(),
      fd_(-1) {
}

MemoryFile::MemoryFile(
    const std::string& path,
    const std::string& mimetype,
    const BufferRef& data,
    UnixTime mtime)
    : File(path, mimetype),
      mtime_(mtime.unixtime()),
      inode_(0),
      size_(data.size()),
      etag_(std::to_string(data.hash())),
      fspath_(),
      fd_(-1) {
#if defined(XZERO_OS_UNIX)
#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  fspath_ = FileUtil::joinPaths(FileUtil::tempDirectory(), "memfile.XXXXXXXX");

  fd_ = mkstemp(const_cast<char*>(fspath_.c_str()));
  if (fd_ < 0)
    RAISE_ERRNO(errno);

  if (ftruncate(fd_, size()) < 0)
    RAISE_ERRNO(errno);

  ssize_t n = pwrite(fd_, data.data(), data.size(), 0);
  if (n < 0)
    RAISE_ERRNO(errno);
#else
  // TODO: URL-escape instead
  fspath_ = path;
  StringUtil::replaceAll(&fspath_, "/", "\%2f");

  if (fspath_.size() >= NAME_MAX)
    RAISE_ERRNO(ENAMETOOLONG);

  FileDescriptor fd = shm_open(fspath_.c_str(), O_RDWR | O_CREAT, 0600);
  if (fd < 0)
    RAISE_ERRNO(errno);

  if (ftruncate(fd, size()) < 0)
    RAISE_ERRNO(errno);

  ssize_t n = pwrite(fd, data.data(), data.size(), 0);
  if (n < 0)
    RAISE_ERRNO(errno);
#endif

  if (static_cast<size_t>(n) != data.size())
    RAISE_ERRNO(EIO);
#elif defined(XZERO_OS_WINDOWS)
#error "TODO"
#else
#error "Not Implemented"
#endif
}

MemoryFile::~MemoryFile() {
#if defined(XZERO_OS_UNIX)
#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  if (fd_ >= 0) {
    FileUtil::close(fd_);
  }
#else
  shm_unlink(fspath_.c_str());
#endif
#elif defined(XZERO_OS_WINDOWS)
  // TODO
#else
#error "Not Implemented"
#endif
}

const std::string& MemoryFile::etag() const {
  return etag_;
}

size_t MemoryFile::size() const noexcept {
  return size_;
}

time_t MemoryFile::mtime() const noexcept {
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

int MemoryFile::createPosixChannel(OpenFlags oflags, int mode) {
  if (fd_ < 0) {
    errno = ENOENT;
    return -1;
  }

#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  // XXX when using dup(fd_) we'd also need to fcntl() the flags.
  // - Both having advantages / disadvantages.
  if (mode)
    return ::open(fspath_.c_str(), to_posix(oflags), mode);
  else
    return ::open(fspath_.c_str(), to_posix(oflags));
#else
  return shm_open(fspath_.c_str(), to_posix(oflags), mode ? mode : 0600);
#endif
}

} // namespace xzero
