// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/sysconfig.h>
#include <x0/Buffer.h>
#include <x0/http/HttpFile.h>
#include <x0/http/HttpFileRef.h>
#include <ev++.h>

#if defined(HAVE_SYS_INOTIFY_H)
#include <sys/inotify.h>
#include <sys/fcntl.h>
#endif

namespace x0 {

class X0_API HttpFileMgr {
  HttpFileMgr(const HttpFileMgr&) = delete;
  HttpFileMgr& operator=(const HttpFileMgr&) = delete;

 public:
  struct Settings;

  HttpFileMgr(struct ev_loop* loop, Settings* settings);
  ~HttpFileMgr();

  void stop();

  HttpFileRef query(const BufferRef& path);
  HttpFileRef query(const std::string& path);

  HttpFileRef get(const std::string& path) { return query(path); }
  HttpFileRef operator()(const BufferRef& path) { return query(path); }
  HttpFileRef operator()(const std::string& path) { return query(path); }
  HttpFileRef operator[](const std::string& path) { return query(path); }

  void release(HttpFile* file);

 private:
  ev::loop_ref loop_;

#if defined(HAVE_SYS_INOTIFY_H)
  int handle_;  //!< inotify handle
  ev::io inotify_;
  std::unordered_map<int, HttpFile*> inotifies_;

  void onFileChanged(ev::io& w, int revents);
#endif

  const Settings* settings_;
  std::unordered_map<std::string, HttpFileRef> cache_;

  friend class HttpFile;
};

struct X0_API HttpFileMgr::Settings {
  bool etagConsiderMtime;
  bool etagConsiderSize;
  bool etagConsiderInode;

  unsigned cacheTTL;

  std::unordered_map<std::string, std::string> mimetypes;
  std::string defaultMimetype;

  explicit Settings(const std::string& mimefile = "")
      : etagConsiderMtime(true),
        etagConsiderSize(true),
        etagConsiderInode(false),
        cacheTTL(10),
        mimetypes(),
        defaultMimetype("text/plain") {
    if (!mimefile.empty()) {
      openMimeTypes(mimefile);
    }
  }

  bool openMimeTypes(const std::string& filename);
};

}  // namespace x0
