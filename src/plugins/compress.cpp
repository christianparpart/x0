/* <x0/plugins/compress.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/range_def.hpp>
#include <x0/strutils.hpp>
#include <x0/io/compress_filter.hpp>
#include <x0/types.hpp>
#include <x0/sysconfig.h>

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

#if !defined(HAVE_BZLIB_H) && !defined(HAVE_ZLIB_H)
#	warning No compression headers available!
#endif

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

	struct context
	{
		std::vector<std::string> content_types_;
		int level_;
		long long min_size_;
		long long max_size_;

		context() :
			content_types_(),
			level_(9),						// best compression
			min_size_(1024),				// 1 KB
			max_size_(128 * 1024 * 1024)	// 128 MB
		{
		}

		bool contains_mime(const std::string& value) const
		{
			for (auto i = content_types_.begin(), e = content_types_.end(); i != e; ++i)
				if (*i == value)
					return true;

			return false;
		}
	};

	virtual void configure()
	{
		context *cx = server_.create_context<context>(this);

		server_.config()["Compress"]["ContentTypes"].load(cx->content_types_);
		server_.config()["Compress"]["Level"].load(cx->level_);
		server_.config()["Compress"]["MinSize"].load(cx->min_size_);
		server_.config()["Compress"]["MaxSize"].load(cx->max_size_);
	}

private:
	void post_process(x0::request *in, x0::response *out)
	{
		if (out->headers.contains("Content-Encoding"))
			return; // do not double-encode content

		const context *cx = server_.context<context>(this);
		if (!cx)
			return;

		long long size = 0;
		if (out->headers.contains("Content-Length"))
			size = boost::lexical_cast<int>(out->headers["Content-Length"]);

		if (size < cx->min_size_)
			return;

		if (size > cx->max_size_)
			return;

		if (!cx->contains_mime(out->headers("Content-Type")))
			return;

		if (x0::buffer_ref r = in->header("Accept-Encoding"))
		{
			typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

			std::vector<std::string> items(x0::split<std::string>(r.str(), ", "));

#if defined(HAVE_BZLIB_H)
			if (std::find(items.begin(), items.end(), "bzip2") != items.end())
			{
				out->headers.push_back("Content-Encoding", "bzip2");
				out->filter_chain.push_back(std::make_shared<x0::bzip2_filter>(cx->level_));
			}
			else
#endif
#if defined(HAVE_ZLIB_H)
			if (std::find(items.begin(), items.end(), "gzip") != items.end())
			{
				out->headers.push_back("Content-Encoding", "gzip");
				out->filter_chain.push_back(std::make_shared<x0::gzip_filter>(cx->level_));
			}
			else if (std::find(items.begin(), items.end(), "deflate") != items.end())
			{
				out->headers.push_back("Content-Encoding", "deflate");
				out->filter_chain.push_back(std::make_shared<x0::deflate_filter>(cx->level_));
			}
			else
#endif
				return;

			// response might change according to Accept-Encoding
			if (!out->headers.contains("Vary"))
				out->headers.push_back("Vary", "Accept-Encoding");
			else
				out->headers["Vary"] += ",Accept-Encoding";

			// removing content-length implicitely enables chunked encoding
			out->headers.remove("Content-Length");
		}
	}
};

X0_EXPORT_PLUGIN(compress);
