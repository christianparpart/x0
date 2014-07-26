// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/*
 * MISSION:
 *
 * implement a WebDAV protocol to be used as NFS-competive replacement,
 * supporting efficient networked I/O, including partial PUTs (Content-Range)
 *
 * Should be usable as an NFS-replacement at our company at least.
 */

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpHeader.h>
#include <base/io/BufferSource.h>
#include <base/io/FileSink.h>
#include <base/strutils.h>
#include <base/Types.h>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

using namespace xzero;
using namespace flow;
using namespace base;

#define TRACE(msg...) DEBUG("hello: " msg)

namespace WebDAV {  // {{{

/*! implements WebDAV's HTTP PUT method.
 *
 * Uploads can be tested e.g. via:
 *   curl -kv -X PUT --data-binary "@/etc/passwd" http://localhost:8081/test.txt
 */
class Put {
 private:
  HttpRequest* request_;
  int fd_;
  bool created_;

 public:
  Put(HttpRequest* r) : request_(r), fd_(-1), created_(false) {}

  bool execute() {
    TRACE("Put.file: %s\n", request_->fileinfo->path().c_str());

    if (request_->contentAvailable()) {
      created_ = !request_->fileinfo->exists();

      if (!created_) ::unlink(request_->fileinfo->path().c_str());

      fd_ =
          ::open(request_->fileinfo->path().c_str(), O_WRONLY | O_CREAT, 0666);
      if (fd_ < 0) {
        perror("WebDav.Put(open)");
        request_->status = HttpStatus::Forbidden;
        request_->finish();
        delete this;
        return true;
      }

      FileSink sink(fd_, true);
      request_->body()->sendto(sink);
      request_->finish();
      delete this;
    } else {
      request_->status = HttpStatus::NotImplemented;
      request_->finish();
      delete this;
    }
    return true;
  }
};

}  // }}}

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class WebDAVPlugin : public x0d::XzeroPlugin {
 public:
  WebDAVPlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    mainHandler("webdav", &WebDAVPlugin::handleRequest);
  }

  ~WebDAVPlugin() {}

 private:
  bool handleRequest(HttpRequest* r, flow::FlowVM::Params& args) {
    if (r->method == "GET") {
      return todo(r);
    } else if (r->method == "PUT") {
      return (new WebDAV::Put(r))->execute();
    } else if (r->method == "MKCOL") {
      return todo(r);
    } else if (r->method == "DELETE") {
      return todo(r);
    } else {
      r->status = HttpStatus::MethodNotAllowed;
      r->finish();
      return true;
    }
  }

  bool todo(HttpRequest* r) {
    r->status = HttpStatus::NotImplemented;
    r->finish();
    return true;
  }
};

X0D_EXPORT_PLUGIN_CLASS(WebDAVPlugin)
