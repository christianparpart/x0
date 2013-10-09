/* <x0/plugins/debug.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/daemon/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/io/BufferSource.h>
#include <x0/Process.h>

/**
 * \ingroup plugins
 * \brief plugin with some debugging/testing helpers
 */
class DebugPlugin :
	public x0::XzeroPlugin
{
public:
	DebugPlugin(x0::XzeroDaemon* d, const std::string& name) :
		x0::XzeroPlugin(d, name)
	{
		registerHandler<DebugPlugin, &DebugPlugin::slowResponse>("debug.slow_response");
		registerHandler<DebugPlugin, &DebugPlugin::dumpCore>("debug.coredump");
		registerHandler<DebugPlugin, &DebugPlugin::dumpCorePost>("debug.coredump.post");
	}

private:
	bool dumpCore(x0::HttpRequest* r, const x0::FlowParams& args)
	{
		r->status = x0::HttpStatus::Ok;
		r->responseHeaders.push_back("Content-Type", "text/plain; charset=utf8");

		x0::Buffer buf;
		buf << "Dumping core\n";
		r->write<x0::BufferSource>(std::move(buf));

		r->finish();

		x0::Process::dumpCore();

		return true;
	}

	bool dumpCorePost(x0::HttpRequest* r, const x0::FlowParams& args)
	{
		r->status = x0::HttpStatus::Ok;
		r->responseHeaders.push_back("Content-Type", "text/plain; charset=utf8");

		x0::Buffer buf;
		buf << "Dumping core\n";
		r->write<x0::BufferSource>(std::move(buf));

		r->finish();

		server().workers()[0]->post<DebugPlugin, &DebugPlugin::_dumpCore>(this);

		return true;
	}

	void _dumpCore()
	{
		x0::Process::dumpCore();
	}

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
