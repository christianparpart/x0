// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpHeader.h>
#include <base/io/BufferSource.h>
#include <base/strutils.h>
#include <base/Types.h>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define TRACE(msg...) DEBUG("hello: " msg)

using namespace base;
using namespace flow;
using namespace xzero;

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class HelloPlugin : public x0d::XzeroPlugin {
 public:
  HelloPlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    mainHandler("hello_example", &HelloPlugin::handleRequest);
  }

  ~HelloPlugin() {}

 private:
  bool handleRequest(HttpRequest* r, flow::vm::Params& args) {
    // set response status code
    r->status = HttpStatus::Ok;

    // set some custom response header
    r->responseHeaders.push_back("Hello", "World");

    // write some content to the client
    r->write<BufferSource>("Hello, World\n");

    // invoke finish() marking this request as being fully handled (response
    // fully generated).
    r->finish();

    // yes, we are handling this request
    return true;
  }
};

X0D_EXPORT_PLUGIN_CLASS(HelloPlugin)
