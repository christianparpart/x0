// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <base/CustomDataMgr.h>

#include <string>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <ev++.h>

namespace xzero {

using namespace base;

class HttpFileMgr;

/**
 * Abstracts a static file to be served by the HTTP stack, providing some bonus
 *properties.
 *
 * This class is meant to speed up getting and computing file properties, such
 *as
 * mimetype, etag, and last-modified string versions, as required to be sent by
 *an
 * HTTP server.
 *
 * Although, the file resource handle, used to serve the actual content is
 *shared
 * and re-used if there are concurrent requests to this file, reducing the
 *number
 * of file descriptor resources in use and speeding up serving as we spare
 * the system-calls to open+fstat+close, too.
 *
 * In order to properly invalidate cached properties, each file object has a TTL
 * until the cached properties have to be regenerated.
 * On systems that support file system change notifications (e.g. inotify),
 * will get property invalidate in realtime.
 *
 * A user application can attach custom data to it that gets also automatically
 * cleared upon file properties invalidation.
 *
 * @see HttpFileMgr
 * @see HttpFileRef
 * @see CustomData
 */
class XZERO_API HttpFile {
  CUSTOMDATA_API_INLINE
 private:
  HttpFileMgr* mgr_;
  std::string path_;
  int fd_;
  struct stat stat_;
  int refs_;
  int errno_;

#if defined(HAVE_SYS_INOTIFY_H)
  int inotifyId_;
#endif
  ev::tstamp cachedAt_;

  mutable std::string etag_;
  mutable std::string mtime_;
  mutable std::string mimetype_;

  HttpFile(const HttpFile&) = delete;
  HttpFile& operator=(const HttpFile&) = delete;

  friend class HttpFileMgr;

 public:
  HttpFile(const std::string& path, HttpFileMgr* mgr);
  ~HttpFile();

  bool open();
  bool update();
  void clearCache();
  void close();

  bool isValid() const;

  bool exists() const { return errno_ == 0; }
  int error() const { return errno_; }

  int handle() const {
    if (fd_ < 0) const_cast<HttpFile*>(this)->open();
    return fd_;
  }

  // property accessors
  operator const struct stat*() const { return &stat_; }
  const std::string& path() const { return path_; }
  std::string filename() const;
  const std::string& etag() const;
  const std::string& lastModified() const;
  const std::string& mimetype() const;

  std::size_t size() const { return stat_.st_size; }
  time_t mtime() const { return stat_.st_mtime; }

  bool isDirectory() const { return S_ISDIR(stat_.st_mode); }
  bool isRegular() const { return S_ISREG(stat_.st_mode); }
  bool isExecutable() const {
    return stat_.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
  }
  const struct stat* operator->() const { return &stat_; }

  void ref();
  void unref();
};

}  // namespace xzero
