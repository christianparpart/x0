// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Buffer.h>
#include <xzero/UnixTime.h>
#include <functional>
#include <string>
#include <memory>
#include <iosfwd>
#include <fcntl.h> // O_* flags for createPosixChannel()

namespace xzero {

/**
 * HTTP servable file.
 *
 * @see FileHandler
 * @see LocalFile, MemoryFile
 * @see FileRepository
 */
class File {
 public:
  File(const File&) = delete;
  File& operator=(const File&) = delete;

  File(const std::string& path, const std::string& mimetype);

  virtual ~File();

  std::string filename() const;
  const std::string& path() const { return path_; }
  const std::string& mimetype() const { return mimetype_; }
  const std::string& lastModified() const;

  virtual const std::string& etag() const = 0;

  virtual size_t size() const noexcept = 0;
  virtual UnixTime mtime() const noexcept = 0;
  virtual size_t inode() const noexcept = 0;
  virtual bool isRegular() const noexcept = 0;
  virtual bool isDirectory() const noexcept = 0;
  virtual bool isExecutable() const noexcept = 0;

  /**
   * Flags that can be passed when creating a system file handle.
   *
   * @see createPosixChannel(OpenFlags oflags)
   */
  enum OpenFlags {
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

  /**
   * Converts given OpenFlags to POSIX compatible flags.
   */
  static int to_posix(OpenFlags oflags);

  /** Creates a POSIX file handle with given flags.
   *
   * @param oflags such as O_RDONLY or O_NONBLOCK, etc (from <fcntl.h>)
   */
  virtual int createPosixChannel(OpenFlags oflags) = 0;

  /**
   * Sets file-error code, that is only used for the validity of the entity.
   */
  void setErrorCode(int ec) { errno_ = ec; }

  /**
   * Retrieves errno-compatible error code for the validity of the entity.
   */
  int errorCode() const noexcept { return errno_; }

  /**
   * Tests wheather this file exists, that is, no error had occurred during
   * vfs_stat() operation.
   */
  bool exists() const noexcept { return errno_ == 0; }

 private:
  std::string path_;
  int errno_;

 protected:
  std::string mimetype_;
  mutable std::string lastModified_;
};

File::OpenFlags operator|(File::OpenFlags a, File::OpenFlags b);

// {{{ inlines
inline File::OpenFlags operator|(File::OpenFlags a, File::OpenFlags b) {
  return (File::OpenFlags) ((unsigned) a | (unsigned) b);
}
// }}}

} // namespace xzero
