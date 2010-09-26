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
	public x0::HttpPlugin
{
public:
	echo_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<echo_plugin, &echo_plugin::handleRequest>("echo_example");
	}

	~echo_plugin()
	{
	}

private:
	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		// set response status code
		out->status = x0::http_error::ok;

		// set response header "Content-Length",
		// if request content were not encoded
		// and if we've received its request header "Content-Length"
		if (!in->header("Content-Encoding"))
			if (x0::BufferRef value = in->header("Content-Length"))
				out->headers.set("Content-Length", value.str());

		// try to read content (if available) and pass it on to our onContent handler,
		// or fall back to just write HELLO (if no request content body was sent).

		if (!in->read(std::bind(&echo_plugin::onContent, this, std::placeholders::_1, in, out))) {
			out->write(
				std::make_shared<x0::BufferSource>("I'm an HTTP echo-server, dude.\n"),
				std::bind(&x0::HttpResponse::finish, out)
			);
		}

		// yes, we are handling this request
		return true;
	}

	// Handler, invoked on request content body chunks,
	// which we want to "echo" back to the client.
	//
	// NOTE, this can be invoked multiple times, depending on the input.
	void onContent(x0::BufferRef&& chunk, x0::HttpRequest *in, x0::HttpResponse *out)
	{
		TRACE("onContent('%s')", chunk.str().c_str());
		out->write(
			std::make_shared<x0::BufferSource>(std::move(chunk)),
			std::bind(&echo_plugin::contentWritten, this, in, out)
		);
	}

	// Handler, invoked when a content chunk has been fully written to the client
	// (or an error occurred).
	//
	// We will try to read another input chunk to echo back to the client,
	// or just finish the response if failed.
	void contentWritten(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		// TODO (write-)error handling; pass errno to this fn, too.

		// try to read another input chunk.
		if (!in->read(std::bind(&echo_plugin::onContent, this, std::placeholders::_1, in, out)))
		{
			// could not read another input chunk, so finish processing this request.
			return out->finish();
		}
	}
};

X0_EXPORT_PLUGIN(echo);
