/* <x0/plugins/hello_example.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define TRACE(msg...) DEBUG("hello: " msg)

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class HelloPlugin :
	public x0::HttpPlugin
{
public:
	HelloPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<HelloPlugin, &HelloPlugin::handleRequest>("hello_example");
	}

	~HelloPlugin()
	{
	}

private:
	virtual bool handleRequest(x0::HttpRequest *r, const x0::FlowParams& args)
	{
		// set response status code
		r->status = x0::HttpError::Ok;

		// set some custom response header
		r->responseHeaders.push_back("Hello", "World");

		// write some content to the client
		r->write<x0::BufferSource>("Hello, World\n");

		// invoke finish() marking this request as being fully handled (response fully generated).
		r->finish();

		// yes, we are handling this request
		return true;
	}
};

X0_EXPORT_PLUGIN_CLASS(HelloPlugin)
