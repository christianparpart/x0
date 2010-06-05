/* <hello.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
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
private:
	x0::request_handler::connection c;

	struct context
	{
		bool enabled;
		std::string hello;
	};

public:
	hello_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		// register content generator
		c = server_.generate_content.connect(&hello_plugin::hello, this);
	}

	~hello_plugin()
	{
		c.disconnect(); // optional, as it gets invoked on ~connection(), too.
	}

private:
	void hello(x0::request_handler::invokation_iterator next, x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (!x0::equals(in->path, "/hello"))
			return next(); // pass request to next handler

		out->status = x0::http_error::ok;
		out->headers.set("Hello", "World");

		if (in->content_available())
		{
			TRACE("content expected");
			in->read(std::bind(&hello_plugin::post, this, std::placeholders::_1, next, in, out));
		}
		else
		{
			TRACE("NO content expected");
			out->write(
				std::make_shared<x0::BufferSource>("Hello, World\n"),
				std::bind(&hello_plugin::done, this, next)
			);
		}
	}

	void post_next(x0::request_handler::invokation_iterator next, x0::HttpRequest *in, x0::HttpResponse *out)
	{

		if (!in->read(std::bind(&hello_plugin::post, this, std::placeholders::_1, next, in, out)))
		{
			TRACE("request content processing: continue");
			return next.done();
		}
		else
			TRACE("request content processing: finished");
	}

	void post(x0::BufferRef&& chunk, x0::request_handler::invokation_iterator next, x0::HttpRequest *in, x0::HttpResponse *out)
	{
		TRACE("post('%s')\n", chunk.str().c_str());
		if (chunk.empty())
			return next.done();

		x0::Buffer reply;
		reply.push_back(chunk);
	//	reply.push_back("\r\n");

		out->write(
			std::make_shared<x0::BufferSource>(reply),
			std::bind(&hello_plugin::post_next, this, next, in, out)
		);
	}

	void done(x0::request_handler::invokation_iterator next)
	{
		// we're done processing this request
		// -> make room for possible more requests to be processed by this connection
		next.done();
	}
};

X0_EXPORT_PLUGIN(hello);
