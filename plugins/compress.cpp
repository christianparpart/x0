/* <x0/plugins/compress.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: filter
 *
 * description:
 *     Dynamically compresses response content stream.
 *     Supported algorithms: deflate, gzip, bzip2.
 *
 * setup API:
 *     string[] compress.types = ['text/html', 'texxt/css',
 *                                'text/plain', 'application/xml',
 *                                'application/xhtml+xml'];
 *     int compress.level = 9;
 *     int compress.min = 64 bytes;
 *     int compress.max = 128 mbyte;
 *
 * request processing API:
 *     none
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/io/CompressFilter.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>

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
	public x0::HttpPlugin
{
private:
	std::vector<std::string> contentTypes_;
	int level_;
	long long minSize_;
	long long maxSize_;

	bool containsMime(const std::string& value) const
	{
		for (auto i = contentTypes_.begin(), e = contentTypes_.end(); i != e; ++i)
			if (*i == value)
				return true;

		return false;
	}

	x0::HttpServer::RequestHook::Connection postProcess_;

public:
	compress_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		contentTypes_(),			// no types
		level_(9),					// best compression
		minSize_(256),				// 256 byte
		maxSize_(128 * 1024 * 1024)	// 128 MB
	{
		contentTypes_.push_back("text/html");
		contentTypes_.push_back("text/css");
		contentTypes_.push_back("text/plain");
		contentTypes_.push_back("application/xml");
		contentTypes_.push_back("application/xhtml+xml");

		postProcess_ = server_.onPostProcess.connect<compress_plugin, &compress_plugin::postProcess>(this);

		registerSetupProperty<compress_plugin, &compress_plugin::setup_types>("compress.types", x0::FlowValue::VOID);
		registerSetupProperty<compress_plugin, &compress_plugin::setup_level>("compress.level", x0::FlowValue::VOID);
		registerSetupProperty<compress_plugin, &compress_plugin::setup_minsize>("compress.min", x0::FlowValue::VOID);
		registerSetupProperty<compress_plugin, &compress_plugin::setup_maxsize>("compress.max", x0::FlowValue::VOID);
	}

	~compress_plugin() {
		server_.onPostProcess.disconnect(postProcess_);
	}

private:
	void setup_types(const x0::FlowParams& args, x0::FlowValue& result)
	{
		contentTypes_.clear();

		for (int i = 0, e = args.size(); i != e; ++i)
			populateContentTypes(args[i]);
	}

	void populateContentTypes(const x0::FlowValue& from)
	{
		switch (from.type()) {
			case x0::FlowValue::STRING:
				contentTypes_.push_back(from.toString());
				break;
			case x0::FlowValue::ARRAY:
				for (auto p: from.toArray())
					populateContentTypes(p);
				break;
			default:
				;
		}
	}

	void setup_level(const x0::FlowParams& args, x0::FlowValue& result)
	{
		level_ = args[0].toNumber();
		level_ = std::min(std::max(level_, 0), 10);
	}

	void setup_minsize(const x0::FlowParams& args, x0::FlowValue& result)
	{
		minSize_ = args[0].toNumber();
	}

	void setup_maxsize(const x0::FlowParams& args, x0::FlowValue& result)
	{
		maxSize_ = args[0].toNumber();
	}

private:
	void postProcess(x0::HttpRequest *in)
	{
		if (in->responseHeaders.contains("Content-Encoding"))
			return; // do not double-encode content

		long long size = 0;
		if (in->responseHeaders.contains("Content-Length"))
			size = std::atoll(in->responseHeaders["Content-Length"].c_str());

		bool chunked = in->responseHeaders["Transfer->Encoding"] == "chunked";

		if (size < minSize_ && !(size <= 0 && chunked))
			return;

		if (size > maxSize_)
			return;

		if (!containsMime(in->responseHeaders["Content-Type"]))
			return;

		if (x0::BufferRef r = in->requestHeader("Accept-Encoding")) {
			std::vector<std::string> items(x0::split<std::string>(r.str(), ", "));

#if defined(HAVE_BZLIB_H)
			if (std::find(items.begin(), items.end(), "bzip2") != items.end()) {
				in->responseHeaders.push_back("Content-Encoding", "bzip2");
				in->outputFilters.push_back(std::make_shared<x0::BZip2Filter>(level_));
			} else
#endif
#if defined(HAVE_ZLIB_H)
			if (std::find(items.begin(), items.end(), "gzip") != items.end()) {
				in->responseHeaders.push_back("Content-Encoding", "gzip");
				in->outputFilters.push_back(std::make_shared<x0::GZipFilter>(level_));
			} else if (std::find(items.begin(), items.end(), "deflate") != items.end()) {
				in->responseHeaders.push_back("Content-Encoding", "deflate");
				in->outputFilters.push_back(std::make_shared<x0::DeflateFilter>(level_));
			} else
#endif
				return;

			// response might change according to Accept-Encoding
			if (!in->responseHeaders.contains("Vary"))
				in->responseHeaders.push_back("Vary", "Accept-Encoding");
			else
				in->responseHeaders.append("Vary", ",Accept-Encoding");

			// removing content-length implicitely enables chunked encoding
			in->responseHeaders.remove("Content-Length");
		}
	}
};

X0_EXPORT_PLUGIN(compress)
