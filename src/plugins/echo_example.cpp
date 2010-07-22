/* <x0/plugins/echo_example.cpp>
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

#define TRACE(msg...) DEBUG("echo: " msg)

/**
 * \ingroup plugins
 * \brief echo content generator plugin
 */
class echo_plugin :
	public x0::HttpPlugin,
	public x0::IHttpRequestHandler
{
public:
	echo_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
	}

	~echo_plugin()
	{
	}

private:
	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (!x0::equals(in->path, "/echo"))
			return false; // pass request to next handler

		out->status = x0::http_error::ok;

		if (x0::BufferRef value = in->header("Content-Length"))
			out->headers.set("Content-Length", value.str());

		if (!in->read(std::bind(&echo_plugin::on_content, this, std::placeholders::_1, in, out))) {
			out->write(
				std::make_shared<x0::BufferSource>("I'm an HTTP echo-server, dude.\n"),
				std::bind(&x0::HttpResponse::finish, out)
			);
		}
		return true;
	}

	void on_content(x0::BufferRef&& chunk, x0::HttpRequest *in, x0::HttpResponse *out)
	{
		TRACE("on_content('%s')", chunk.str().c_str());
		out->write(
			std::make_shared<x0::BufferSource>(std::move(chunk)),
			std::bind(&echo_plugin::content_written, this, in, out)
		);
	}

	void content_written(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (!in->read(std::bind(&echo_plugin::on_content, this, std::placeholders::_1, in, out)))
		{
			TRACE("request content processing: done");
			return out->finish();
		}
	}
};

X0_EXPORT_PLUGIN(echo);
