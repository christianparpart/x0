// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero/RuntimeError.h>
#include <fcntl.h>

#if defined(XZERO_OS_WINDOWS)
#include <Windows.h>
#endif

#if defined(XZERO_OS_UNIX)
#include <unistd.h>
#endif

namespace xzero {

/**
 * Flags that can be passed when creating a system file handle.
 *
 * @see createPosixChannel(FileOpenFlags oflags)
 */
enum FileOpenFlags {
  Read        = 0x0001, // O_RDONLY
  Write       = 0x0002, // O_WRONLY
  ReadWrite   = 0x0003, // O_RDWR
  Create      = 0x0004, // O_CREAT
  CreateNew   = 0x0008, // O_EXCL
  Truncate    = 0x0010, // O_TRUNC
  Append      = 0x0020, // O_APPEND
  Share       = 0x0040, // O_CLOEXEC negagted
  NonBlocking = 0x0080, // O_NONBLOCK
  TempFile    = 0x0100, // O_TMPFILE
};

constexpr FileOpenFlags operator|(FileOpenFlags a, FileOpenFlags b) {
  return (FileOpenFlags) ((unsigned) a | (unsigned) b);
}

/**
 * Converts given FileOpenFlags to POSIX compatible flags.
 */
constexpr int to_posix(FileOpenFlags oflags) {
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

/**
  * Represents a system file descriptor that gets automatically closed.
  */
class [[nodiscard]] FileHandle {
 public:
  FileHandle(const FileHandle& other) = delete;
  FileHandle& operator=(const FileHandle& other) = delete;

#if defined(XZERO_OS_WINDOWS)
  using native_type = HANDLE;
  static inline const native_type InvalidHandle = INVALID_HANDLE_VALUE;
#else
  using native_type = int;
  static const native_type InvalidHandle = -1;
#endif

  FileHandle() : handle_{InvalidHandle} {}
  explicit FileHandle(native_type handle) : handle_{handle} {}
  ~FileHandle() { close(); }

  FileHandle(FileHandle&& other) : handle_{other.release()} {}
  FileHandle& operator=(FileHandle&& other) {
    close();
    handle_ = other.release();
    return *this;
  }

  bool isClosed() const noexcept { return handle_ == InvalidHandle; }
  bool isOpen() const noexcept { return handle_ != InvalidHandle; }

  native_type native() const noexcept { return handle_; }
  operator native_type() const noexcept { return handle_; }

  native_type release() {
    native_type n = handle_;
    handle_ = InvalidHandle;
    return n;
  }

  void close();
  FileHandle dup();
  FileHandle dup(FileOpenFlags oflags);

  ssize_t write(const void* buf, size_t count);

 private:
  native_type handle_;
};

// {{{ inlines
inline void FileHandle::close() {
  if (isOpen()) {
#if defined(XZERO_OS_WINDOWS)
    CloseHandle(release());
#else
    ::close(release());
#endif
  }
}

inline ssize_t FileHandle::write(const void* buf, size_t count) {
#if defined(XZERO_OS_UNIX)
  return ::write(handle_, buf, count);
#else
  DWORD nwritten = 0;
  if (!WriteFile(handle_, buf, count, &nwritten, nullptr))
    return -1;
  return static_cast<ssize_t>(nwritten);
#endif
}

inline FileHandle FileHandle::dup() {
#if defined(XZERO_OS_UNIX)
  return FileHandle{::dup(handle_)};
#else
#endif
}

inline FileHandle FileHandle::dup(FileOpenFlags oflags) {
#if defined(XZERO_OS_UNIX)
  FileHandle fd = dup();
  if (fcntl(fd, F_SETFL, to_posix(oflags)) < 0)
    RAISE_ERRNO(errno);

  return fd;
#else
#endif
}

// }}}

}  // namespace xzero
