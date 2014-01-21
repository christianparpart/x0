/* <x0/plugins/expire.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type:
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
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/DateTime.h>
#include <x0/TimeSpan.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#define TRACE(msg...) this->debug(msg)

/**
 * \ingroup plugins
 * \brief adds Expires and Cache-Control response header
 */
class ExpirePlugin :
	public x0d::XzeroPlugin
{
public:
	ExpirePlugin(x0d::XzeroDaemon* d, const std::string& name) :
		x0d::XzeroPlugin(d, name)
	{
		mainFunction("expire", &ExpirePlugin::expire).params(x0::FlowType::Number);
	}

private:
	// void expire(datetime / timespan)
	void expire(x0::HttpRequest *r, x0::FlowVM::Params& args)
	{
		time_t now = r->connection.worker().now().unixtime();
		time_t mtime = r->fileinfo ? r->fileinfo->mtime() : now;
        time_t value = args.get<x0::FlowNumber>(1);

		// passed a timespan
		if (value < mtime)
			value = value + now;

		// (mtime+span) points to past?
		if (value < now)
			value = now;

		r->responseHeaders.overwrite("Expires", x0::DateTime(value).http_str().str());

		x0::Buffer cc(20);
		cc << "max-age=" << (value - now);

		r->responseHeaders.overwrite("Cache-Control", cc.str());
	}
};

X0_EXPORT_PLUGIN_CLASS(ExpirePlugin)
