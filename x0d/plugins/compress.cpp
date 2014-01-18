/* <x0/plugins/compress.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/io/CompressFilter.h>
#include <x0/Tokenizer.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/sysconfig.h>

#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
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
	public x0d::XzeroPlugin
{
private:
	std::unordered_map<std::string, int> contentTypes_;
	int level_;
	long long minSize_;
	long long maxSize_;

	bool containsMime(const std::string& value) const
	{
        return contentTypes_.find(value) != contentTypes_.end();
	}

	x0::HttpServer::RequestHook::Connection postProcess_;

public:
	compress_plugin(x0d::XzeroDaemon* d, const std::string& name) :
		x0d::XzeroPlugin(d, name),
		contentTypes_(),			// no types
		level_(9),					// best compression
		minSize_(256),				// 256 byte
		maxSize_(128 * 1024 * 1024)	// 128 MB
	{
		contentTypes_["text/html"] = 0;
		contentTypes_["text/css"] = 0;
		contentTypes_["text/plain"] = 0;
		contentTypes_["application/xml"] = 0;
		contentTypes_["application/xhtml+xml"] = 0;

		postProcess_ = server().onPostProcess.connect<compress_plugin, &compress_plugin::postProcess>(this);

		setupFunction("compress.types", &compress_plugin::setup_types, x0::FlowType::StringArray);
		setupFunction("compress.level", &compress_plugin::setup_level, x0::FlowType::Number);
		setupFunction("compress.min", &compress_plugin::setup_minsize, x0::FlowType::Number);
		setupFunction("compress.max", &compress_plugin::setup_maxsize, x0::FlowType::Number);
	}

	~compress_plugin() {
		server().onPostProcess.disconnect(postProcess_);
	}

private:
	void setup_types(x0::FlowVM::Params& args)
	{
		contentTypes_.clear();

        const auto& x = *args.get<x0::GCStringArray*>(1);

		for (int i = 0, e = args.size(); i != e; ++i) {
            contentTypes_[x.data()[i].str()] = 0;
        }
	}

	void setup_level(x0::FlowVM::Params& args)
	{
		level_ = args.get<int>(1);
		level_ = std::min(std::max(level_, 0), 10);
	}

	void setup_minsize(x0::FlowVM::Params& args)
	{
		minSize_ = args.get<int>(1);
	}

	void setup_maxsize(x0::FlowVM::Params& args)
	{
		maxSize_ = args.get<int>(1);
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
			auto items = x0::Tokenizer<x0::BufferRef, x0::BufferRef>::tokenize(r, ", ");

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
