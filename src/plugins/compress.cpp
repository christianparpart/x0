/* <x0/plugins/compress.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "compress.h"
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/io/CompressFilter.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

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
	public x0::HttpPlugin,
	public ICompressPlugin
{
private:
	struct context : public x0::ScopeValue
	{
		std::vector<std::string> content_types_;
		int level_;
		long long min_size_;
		long long max_size_;

		context() :
			content_types_(),				// no types
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

		virtual void merge(const x0::ScopeValue *value)
		{
			//if (auto cx = dynamic_cast<const context *>(value))
			{
				; //! \todo
			}
		}
	};

	x0::HttpServer::RequestPostHook::Connection postProcess_;

public:
	compress_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		using namespace std::placeholders;

		postProcess_ = server_.onPostProcess.connect<compress_plugin, &compress_plugin::postProcess>(this);

		declareCVar("CompressTypes", x0::HttpContext::server, &compress_plugin::setup_types);
		declareCVar("CompressLevel", x0::HttpContext::server, &compress_plugin::setup_level);
		declareCVar("CompressMinSize", x0::HttpContext::server, &compress_plugin::setup_minsize);
		declareCVar("CompressMaxSize", x0::HttpContext::server, &compress_plugin::setup_maxsize);
	}

	~compress_plugin() {
		server_.onPostProcess.disconnect(postProcess_);
		server().release(this);
	}

public: // ICompressPlugin
	virtual void setCompressTypes(const std::vector<std::string>& value)
	{
		server().acquire<context>(this)->content_types_ = value;
	}

	virtual void setCompressLevel(int value)
	{
		server().acquire<context>(this)->level_ = value;
	}

	virtual void setCompressMinSize(int value)
	{
		server().acquire<context>(this)->min_size_ = value;
	}

	virtual void setCompressMaxSize(int value)
	{
		server().acquire<context>(this)->max_size_ = value;
	}

private:
	std::error_code setup_types(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<context>(this)->content_types_);
	}

	std::error_code setup_level(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<context>(this)->level_);
	}

	std::error_code setup_minsize(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<context>(this)->min_size_);
	}

	std::error_code setup_maxsize(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<context>(this)->max_size_);
	}

	void postProcess(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (out->headers.contains("Content-Encoding"))
			return; // do not double-encode content

		const context *cx = server_.resolveHost(in->hostid())->get<context>(this);
		if (!cx && !(cx = server().get<context>(this)))
			return;

		long long size = 0;
		if (out->headers.contains("Content-Length"))
			size = boost::lexical_cast<int>(out->headers["Content-Length"]);

		std::string te(out->headers["Transfer-Encoding"]);
		bool chunked = (te == "chunked");

		if (size < cx->min_size_ && !(size <= 0 && chunked))
			return;

		if (size > cx->max_size_)
			return;

		if (!cx->contains_mime(out->headers("Content-Type")))
			return;

		if (x0::BufferRef r = in->header("Accept-Encoding"))
		{
			typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

			std::vector<std::string> items(x0::split<std::string>(r.str(), ", "));

#if defined(HAVE_BZLIB_H)
			if (std::find(items.begin(), items.end(), "bzip2") != items.end())
			{
				out->headers.push_back("Content-Encoding", "bzip2");
				out->filters.push_back(std::make_shared<x0::BZip2Filter>(cx->level_));
			}
			else
#endif
#if defined(HAVE_ZLIB_H)
			if (std::find(items.begin(), items.end(), "gzip") != items.end())
			{
				out->headers.push_back("Content-Encoding", "gzip");
				out->filters.push_back(std::make_shared<x0::GZipFilter>(cx->level_));
			}
			else if (std::find(items.begin(), items.end(), "deflate") != items.end())
			{
				out->headers.push_back("Content-Encoding", "deflate");
				out->filters.push_back(std::make_shared<x0::DeflateFilter>(cx->level_));
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
