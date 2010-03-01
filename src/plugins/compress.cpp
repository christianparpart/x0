/* <x0/mod_debug.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/range_def.hpp>
#include <x0/strutils.hpp>
#include <x0/io/compress_filter.hpp>
#include <x0/types.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>

#include <sstream>
#include <iostream>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class compress_plugin :
	public x0::plugin
{
private:
	x0::server::request_post_hook::connection post_process_;

public:
	compress_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		post_process_ = server_.post_process.connect(boost::bind(&compress_plugin::post_process, this, _1, _2));
	}

	~compress_plugin() {
		server_.post_process.disconnect(post_process_);
	}

	virtual void configure()
	{
	}

private:
	void post_process(x0::request *in, x0::response *out)
	{
		if (out->headers.contains("Content-Encoding"))
			return; // do not double-encode content

		if (x0::buffer_ref r = in->header("Accept-Encoding"))
		{
			typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

			std::vector<std::string> items(x0::split<std::string>(r.str(), ", "));

			if (std::find(items.begin(), items.end(), "gzip") != items.end())
			{
				out->headers.push_back("Content-Encoding", "gzip");
				out->filter_chain.push_back(std::make_shared<x0::compress_filter>(/*gzip*/));
			}
			else if (std::find(items.begin(), items.end(), "deflate") != items.end())
			{
				out->headers.push_back("Content-Encoding", "deflate");
				out->filter_chain.push_back(std::make_shared<x0::compress_filter>(/*deflate*/));
			}
			else
				return;

			// response might change according to Accept-Encoding
			if (!out->headers.contains("Vary"))
				out->headers.push_back("Vary", "Accept-Encoding");
			else
				out->headers["Vary"] += ",Accept-Encoding";

			// removing content-length implicitely enables chunked encoding
			out->headers.remove("Content-Length");

			//! \todo cache compressed result if static file (maybe as part of compress_filter class / sendfile plugin?)
		}
	}
};

X0_EXPORT_PLUGIN(compress);
