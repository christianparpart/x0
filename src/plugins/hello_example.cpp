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
	public x0::HttpPlugin,
	public x0::IHttpRequestHandler
{
private:
	struct context
	{
		bool enabled;
		std::string hello;
	};

public:
	hello_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
	}

	~hello_plugin()
	{
	}

private:
	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (!x0::equals(in->path, "/hello"))
			return false; // pass request to next handler

		out->status = x0::http_error::ok;
		out->headers.set("Hello", "World");

		if (in->content_available())
		{
			TRACE("content expected");
			in->read(std::bind(&hello_plugin::post, this, std::placeholders::_1, in, out));
		}
		else
		{
			TRACE("NO content expected");
			out->write(
				std::make_shared<x0::BufferSource>("Hello, World\n"),
				std::bind(&x0::HttpResponse::finish, out)
			);
		}
		return true;
	}

	void postNext(x0::HttpRequest *in, x0::HttpResponse *out)
	{

		if (!in->read(std::bind(&hello_plugin::post, this, std::placeholders::_1, in, out)))
		{
			TRACE("request content processing: continue");
		}
		else
		{
			TRACE("request content processing: finished");
			out->finish();
		}
	}

	void post(x0::BufferRef&& chunk, x0::HttpRequest *in, x0::HttpResponse *out)
	{
		TRACE("post('%s')\n", chunk.str().c_str());
		if (chunk.empty())
			return out->finish();

		x0::Buffer reply;
		reply.push_back(chunk);
		//reply.push_back("\r\n");

		out->write(
			std::make_shared<x0::BufferSource>(reply),
			std::bind(&hello_plugin::postNext, this, in, out)
		);
	}
};

X0_EXPORT_PLUGIN(hello);
