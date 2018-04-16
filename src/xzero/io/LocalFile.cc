// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/LocalFile.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/MimeTypes.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>

#include <fstream>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#if defined(XZERO_OS_WIN32)
#include <io.h>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define open _open
#endif

#if defined(XZERO_OS_UNIX)
#include <unistd.h>
#endif

#if defined(WIN32) || defined(WIN64)
// Copied from linux libc sys/stat.h:
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

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

size_t LocalFile::size() const noexcept {
  return stat_.st_size;
}

time_t LocalFile::mtime() const noexcept {
  return stat_.st_mtime;
}

size_t LocalFile::inode() const noexcept {
  return stat_.st_ino;
}

bool LocalFile::isRegular() const noexcept {
  return S_ISREG(stat_.st_mode);
}

bool LocalFile::isDirectory() const noexcept {
  return S_ISDIR(stat_.st_mode);
}

bool LocalFile::isExecutable() const noexcept {
#if defined(XZERO_OS_UNIX)
  return stat_.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
#elif defined(XZERO_OS_WIN32)
  return fs::path(path()).extension() == ".exe";
#else
  return false;
#endif
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

  if (rv < 0) {
    setErrorCode(errno);
  } else {
    setErrorCode(0);

    if (isDirectory()) {
      mimetype_ = "inode/directory";
    }
  }
}

int LocalFile::createPosixChannel(OpenFlags oflags, int mode) {
  if (mode) {
    return ::open(path().c_str(), to_posix(oflags), mode);
  } else {
    return ::open(path().c_str(), to_posix(oflags));
  }
}

std::shared_ptr<LocalFile> LocalFile::get(const std::string& path) {
  static MimeTypes mimetypes;
  static LocalFileRepository repo(mimetypes, "/", true, true, false);
  return std::static_pointer_cast<LocalFile>(repo.getFile(path));
}

} // namespace xzero
