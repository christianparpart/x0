/* <x0/mod_example.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/plugin.hpp>
#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <cstring>
#include <cerrno>
#include <cstddef>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define TRACE(msg...) DEBUG("example: " msg)

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class example_plugin :
	public x0::plugin
{
private:
	x0::request_handler::connection c;

	struct context
	{
		bool enabled;
		std::string hello;
	};

public:
	example_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		// register content generator
		c = server_.generate_content.connect(&example_plugin::hello, this);
	}

	~example_plugin()
	{
		c.disconnect(); // optional, as it gets invoked on ~connection(), too.
	}

	virtual void configure()
	{
		// \!todo add configs for custom hello strings and /prefix locations
	}

private:
	void hello(x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out)
	{
		if (!x0::iequals(in->path, "/hello"))
			return next(); // pass request to next handler

		if (in->expect_content())
		{
			TRACE("content expected");
			in->read(std::bind(&example_plugin::post, this,
						std::placeholders::_1, next, in, out));
		}
		else
		{
			TRACE("NO content expected");
			out->write(
				std::make_shared<x0::buffer_source>("Hello, World\n"),
				std::bind(&example_plugin::done, this, next)
			);
		}
	}

	bool post(x0::buffer_ref&& chunk, x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out)
	{
		TRACE("post('%s')\n", chunk.str().c_str());
#if 0
		if (chunk.empty())
		{
			next.done();
			return false;
		}

		x0::buffer reply;
		reply.push_back(chunk);
		reply.push_back("\r\n");

		out->write(
			std::make_shared<x0::buffer_source>(reply),
			std::bind(&example_plugin::post, this, next, in, out)
		);
		return true;
#else
		x0::buffer reply;
		reply.push_back(chunk);
		reply.push_back("\r\n");

		out->write(
			std::make_shared<x0::buffer_source>(reply),
			std::bind(&example_plugin::done, this, next)
		);
		return false;
#endif
	}

	void done(x0::request_handler::invokation_iterator next)
	{
		// we're done processing this request
		// -> make room for possible more requests to be processed by this connection
		next.done();
	}
};

X0_EXPORT_PLUGIN(example);
