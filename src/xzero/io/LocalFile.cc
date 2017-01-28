// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/LocalFile.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/io/MemoryMap.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileInputStream.h>
#include <xzero/io/FileOutputStream.h>
#include <xzero/MimeTypes.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>

#include <fstream>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

namespace xzero {

LocalFile::LocalFile(LocalFileRepository& repo,
                     const std::string& path,
                     const std::string& mimetype)
    : File(path, mimetype),
      repo_(repo),
      stat_(),
      etag_() {
  update();
}

LocalFile::~LocalFile() {
}

size_t LocalFile::size() const XZERO_NOEXCEPT {
  return stat_.st_size;
}

time_t LocalFile::mtime() const XZERO_NOEXCEPT {
  return stat_.st_mtime;
}

size_t LocalFile::inode() const XZERO_NOEXCEPT {
  return stat_.st_ino;
}

bool LocalFile::isRegular() const XZERO_NOEXCEPT {
  return S_ISREG(stat_.st_mode);
}

bool LocalFile::isDirectory() const XZERO_NOEXCEPT {
  return S_ISDIR(stat_.st_mode);
}

bool LocalFile::isExecutable() const XZERO_NOEXCEPT {
  return stat_.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
}

const std::string& LocalFile::etag() const {
  // compute ETag response header value on-demand
  if (etag_.empty()) {
    size_t count = 0;
    Buffer buf(256);
    buf.push_back('"');

    if (repo_.etagConsiderMTime_) {
      if (count++) buf.push_back('-');
      buf.push_back(mtime());
    }

    if (repo_.etagConsiderSize_) {
      if (count++) buf.push_back('-');
      buf.push_back(size());
    }

    if (repo_.etagConsiderINode_) {
      if (count++) buf.push_back('-');
      buf.push_back(inode());
    }

    buf.push_back('"');

    etag_ = buf.str();
  }

  return etag_;
}

void LocalFile::update() {
  int rv = stat(path().c_str(), &stat_);

  if (rv < 0)
    setErrorCode(errno);
  else
    setErrorCode(0);
}

int LocalFile::createPosixChannel(OpenFlags oflags) {
  return ::open(path().c_str(), to_posix(oflags));
}

std::unique_ptr<InputStream> LocalFile::createInputChannel() {
  return std::unique_ptr<InputStream>(new FileInputStream(path()));
}

std::unique_ptr<OutputStream> LocalFile::createOutputChannel(OpenFlags flags, int mode) {
  return std::unique_ptr<OutputStream>(new FileOutputStream(path(), flags, mode));
}

std::unique_ptr<MemoryMap> LocalFile::createMemoryMap(bool rw) {
  // use FileDescriptor for auto-closing here, in case of exceptions
  FileDescriptor fd = ::open(path().c_str(), rw ? O_RDWR : O_RDONLY);
  if (fd < 0)
    RAISE_ERRNO(errno);

  return std::unique_ptr<MemoryMap>(new MemoryMap(fd, 0, size(), rw));
}

std::shared_ptr<LocalFile> LocalFile::get(const std::string& path) {
  static MimeTypes mimetypes;
  static LocalFileRepository repo(mimetypes, "/", true, true, false);
  return std::static_pointer_cast<LocalFile>(repo.getFile(path, "/"));
}

} // namespace xzero
