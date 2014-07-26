// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <base/io/BufferSource.h>
#include <base/Process.h>

using namespace xzero;
using namespace flow;
using namespace base;

/**
 * \ingroup plugins
 * \brief plugin with some debugging/testing helpers
 */
class DebugPlugin : public x0d::XzeroPlugin {
 public:
  DebugPlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    mainHandler("debug.slow_response", &DebugPlugin::slowResponse);
    mainHandler("debug.coredump", &DebugPlugin::dumpCore);
    mainHandler("debug.coredump.post", &DebugPlugin::dumpCorePost);
    mainHandler("debug.dump_request_buffers", &DebugPlugin::dumpRequestBuffers);
  }

 private:
  bool dumpRequestBuffers(HttpRequest* r, vm::Params& args) {
    r->status = HttpStatus::NotFound;

    unsigned long long cid = r->query.toInt();

    for (auto worker : server().workers()) {
      worker->eachConnection([=](HttpConnection* connection)->bool {
        if (connection->id() == cid) {
          if (connection->requestBuffer().empty()) {
            r->status = HttpStatus::NoContent;
          } else {
            r->status = HttpStatus::Ok;
            r->responseHeaders.push_back("Content-Type",
                                         "text/plain; charset=utf8");

            char buf[128];

            snprintf(buf, sizeof(buf), "%zu",
                     connection->requestBuffer().size());
            r->responseHeaders.push_back("Content-Length", buf);

            snprintf(buf, sizeof(buf), "%zu",
                     connection->requestParserOffset());
            r->responseHeaders.push_back("X-RequestParser-Offset", buf);

            r->write<BufferSource>(connection->requestBuffer());
          }
        }
        return true;
      });
    }

    r->finish();

    return true;
  }

  bool dumpCore(HttpRequest* r, flow::vm::Params& args) {
    r->status = HttpStatus::Ok;
    r->responseHeaders.push_back("Content-Type", "text/plain; charset=utf8");

    Buffer buf;
    buf << "Dumping core\n";
    r->write<BufferSource>(std::move(buf));

    r->finish();

    Process::dumpCore();

    return true;
  }

  bool dumpCorePost(HttpRequest* r, flow::vm::Params& args) {
    r->status = HttpStatus::Ok;
    r->responseHeaders.push_back("Content-Type", "text/plain; charset=utf8");

    Buffer buf;
    buf << "Dumping core\n";
    r->write<BufferSource>(std::move(buf));

    r->finish();

    server().workers()[0]->post<DebugPlugin, &DebugPlugin::_dumpCore>(this);

    return true;
  }

  void _dumpCore() { Process::dumpCore(); }

  bool slowResponse(HttpRequest* r, flow::vm::Params& args) {
    const unsigned count = 8;
    for (unsigned i = 0; i < count; ++i) {
      Buffer buf;
      buf << "slow response: " << i << "/" << count << "\n";

      if (i) sleep(1);  // yes! make it slow!

      printf(": %s", buf.c_str());
      r->write<BufferSource>(std::move(buf));
    }
    r->finish();
    return true;
  }
};

X0D_EXPORT_PLUGIN_CLASS(DebugPlugin)
