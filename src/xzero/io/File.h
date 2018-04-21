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
#include <xzero/io/FileHandle.h>

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>

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

  /** Creates a POSIX file handle with given flags.
   *
   * @param oflags such as O_RDONLY or O_NONBLOCK, etc (from <fcntl.h>)
   */
  virtual FileHandle createPosixChannel(FileOpenFlags oflags) = 0;

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

} // namespace xzero
