/* <x0/plugins/filter_example.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/io/Filter.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

// {{{ ExampleFilter
class ExampleFilter :
	public x0::Filter
{
public:
	ExampleFilter();

	virtual x0::Buffer process(const x0::BufferRef& input, bool eof);
};

ExampleFilter::ExampleFilter()
{
}

x0::Buffer ExampleFilter::process(const x0::BufferRef& input, bool eof)
{
	x0::Buffer result;

#if 1
	// return identity
	result.push_back(input);
#else
	// return upper-case
	for (auto i = input.begin(), e = input.end(); i != e; ++i)
		result.push_back(static_cast<char>(std::toupper(*i)));
#endif

	return result;
}
// }}}

#define TRACE(msg...) DEBUG("filter_example: " msg)

/**
 * \ingroup plugins
 * \brief ...
 */
class filter_plugin :
	public x0::HttpPlugin
{
private:
	struct context : public x0::ScopeValue
	{
		bool enabled;

		context() :
			enabled(false)
		{
		}

		virtual void merge(const x0::ScopeValue *value)
		{
		}
	};

	x0::HttpServer::RequestPostHook::Connection postProcess_;

public:
	filter_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		using namespace std::placeholders;

		postProcess_ = server_.onPostProcess.connect<filter_plugin, &filter_plugin::postProcess>(this);

		TRACE("filter_plugin()");
		declareCVar("FilterExample", x0::HttpContext::server|x0::HttpContext::host, &filter_plugin::setup_enabled);
	}

	~filter_plugin() {
		server_.onPostProcess.disconnect(postProcess_);
		server().release(this);
	}

private:
	std::error_code setup_enabled(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		TRACE("setup_enabled(val)");
		return cvar.load(s.acquire<context>(this)->enabled);
	}

	void postProcess(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		const context *cx = server_.resolveHost(in->hostid())->get<context>(this);
		if (!cx && !(cx = server().get<context>(this)))
			return;

		if (!cx->enabled)
			return;

		TRACE("postProcess...");
		out->headers.push_back("Content-Encoding", "filter_example");
		out->filters.push_back(std::make_shared<ExampleFilter>());

		// response might change according to Accept-Encoding
		if (!out->headers.contains("Vary"))
			out->headers.push_back("Vary", "Accept-Encoding");
		else
			out->headers["Vary"] += ",Accept-Encoding";

		// removing content-length implicitely enables chunked encoding
		out->headers.remove("Content-Length");
	}
};

X0_EXPORT_PLUGIN(filter);
