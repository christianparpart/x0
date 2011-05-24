/* <x0/plugins/debug.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>

/**
 * \ingroup plugins
 * \brief plugin with some debugging/testing helpers
 */
class DebugPlugin :
	public x0::HttpPlugin
{
public:
	DebugPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<DebugPlugin, &DebugPlugin::slowResponse>("debug.slow_response");
	}

private:
	bool slowResponse(x0::HttpRequest* r, const x0::FlowParams& args)
	{
		const unsigned count = 8;
		for (unsigned i = 0; i < count; ++i) {
			x0::Buffer buf;
			buf << "slow response: " << i << "/" << count << "\n";

			if (i)
				sleep(1); // yes! make it slow!

			printf(": %s", buf.c_str());
			r->write<x0::BufferSource>(std::move(buf));
		}
		r->finish();
		return true;
	}
};

X0_EXPORT_PLUGIN_CLASS(DebugPlugin)
