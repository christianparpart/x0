/* <x0/plugins/hello_example.cpp>
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
class hello_plugin :
	public x0::HttpPlugin
{
public:
	hello_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<hello_plugin, &hello_plugin::handleRequest>("hello_example");
	}

	~hello_plugin()
	{
	}

private:
	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		// set response status code
		out->status = x0::HttpError::Ok;

		// set some custom response header
		out->responseHeaders.push_back("Hello", "World");

		// write some content to the client, and invoke
		// HttpResponse::finish on completion, thus, finish processing this request.
		out->write(
			std::make_shared<x0::BufferSource>("Hello, World\n"),
			std::bind(&x0::HttpResponse::finish, out)
		);

		// yes, we are handling this request
		return true;
	}
};

X0_EXPORT_PLUGIN(hello)
