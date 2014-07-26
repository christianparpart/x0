// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/* plugin type:
 *     metadata
 *
 * description:
 *     Adds Expires and Cache-Control headers to the response.
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     void expires(absolute_time_or_timespan_from_now);
 *
 * examples:
 *     handler main {
 *         docroot '/srv/www'
 *
 *         if phys.exists
 *             expire phys.mtime + 30 days
 *         else
 *             expire sys.now + 30 secs
 *
 *         staticfile
 *     }
 *
 *     handler main {
 *         docroot '/srv/www'
 *         expire 30 days if phys.exists and not phys.path =$ '.csp'
 *         staticfile
 *     }
 */

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpHeader.h>
#include <base/DateTime.h>
#include <base/TimeSpan.h>
#include <base/strutils.h>
#include <base/Types.h>

#define TRACE(msg...) this->debug(msg)

using namespace xzero;
using namespace flow;
using namespace base;

/**
 * \ingroup plugins
 * \brief adds Expires and Cache-Control response header
 */
class ExpirePlugin : public x0d::XzeroPlugin {
 public:
  ExpirePlugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name) {
    mainFunction("expire", &ExpirePlugin::expire).params(flow::FlowType::Number);
  }

 private:
  // void expire(datetime / timespan)
  void expire(HttpRequest* r, flow::vm::Params& args) {
    time_t now = r->connection.worker().now().unixtime();
    time_t mtime = r->fileinfo ? r->fileinfo->mtime() : now;
    time_t value = args.getInt(1);

    // passed a timespan
    if (value < mtime) value = value + now;

    // (mtime+span) points to past?
    if (value < now) value = now;

    r->responseHeaders.overwrite("Expires",
                                 DateTime(value).http_str().str());

    Buffer cc(20);
    cc << "max-age=" << (value - now);

    r->responseHeaders.overwrite("Cache-Control", cc.str());
  }
};

X0D_EXPORT_PLUGIN_CLASS(ExpirePlugin)
