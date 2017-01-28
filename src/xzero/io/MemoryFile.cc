// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/MemoryFile.h>
#include <xzero/io/MemoryMap.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileInputStream.h>
#include <xzero/io/FileOutputStream.h>
#include <xzero/Buffer.h>
#include <xzero/hash/FNV.h>
#include <xzero/io/FileUtil.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>

#include <fstream>
#include <ctime>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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
      etag_(),
      fspath_() {
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
}

MemoryFile::~MemoryFile() {
#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  if (fd_ >= 0) {
    FileUtil::close(fd_);
  }
#else
  shm_unlink(fspath_.c_str());
#endif
}

const std::string& MemoryFile::etag() const {
  return etag_;
}

size_t MemoryFile::size() const XZERO_NOEXCEPT {
  return size_;
}

time_t MemoryFile::mtime() const XZERO_NOEXCEPT {
  return mtime_;
}

size_t MemoryFile::inode() const XZERO_NOEXCEPT {
  return inode_;
}

bool MemoryFile::isRegular() const XZERO_NOEXCEPT {
  return true;
}

bool MemoryFile::isDirectory() const XZERO_NOEXCEPT {
  return false;
}

bool MemoryFile::isExecutable() const XZERO_NOEXCEPT {
  return false;
}

int MemoryFile::createPosixChannel(OpenFlags oflags) {
#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  if (fd_ < 0) {
    errno = ENOENT;
    return -1;
  }

  // XXX when using dup(fd_) we'd also need to fcntl() the flags.
  // - Both having advantages / disadvantages.
  return ::open(fspath_.c_str(), to_posix(oflags));
#else
  return shm_open(fspath_.c_str(), to_posix(oflags), 0600);
#endif
}

std::unique_ptr<InputStream> MemoryFile::createInputChannel() {
#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  return std::unique_ptr<InputStream>(new FileInputStream(fspath_));
#else
  RAISE(RuntimeError, "Not implemented.");
#endif
}

std::unique_ptr<OutputStream> MemoryFile::createOutputChannel(File::OpenFlags flags, int mode) {
#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  return std::unique_ptr<OutputStream>(new FileOutputStream(fspath_, flags, mode));
#else
  RAISE(RuntimeError, "Not implemented.");
#endif
}

std::unique_ptr<MemoryMap> MemoryFile::createMemoryMap(bool rw) {
#if defined(XZERO_MEMORYFILE_USE_TMPFILE)
  // use FileDescriptor for auto-closing here, in case of exceptions
  FileDescriptor fd = ::open(fspath_.c_str(), rw ? O_RDWR : O_RDONLY);
  if (fd < 0)
    RAISE_ERRNO(errno);

  return std::unique_ptr<MemoryMap>(new MemoryMap(fd, 0, size(), rw));
#else
  return std::unique_ptr<MemoryMap>(new MemoryMap(fd_, 0, size(), rw));
#endif
}

} // namespace xzero
