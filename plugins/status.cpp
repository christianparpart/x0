/* <x0/plugins/status.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/TimeSpan.h>
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

#define TRACE(msg...) DEBUG("status: " msg)

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class StatusPlugin :
	public x0::HttpPlugin
{
public:
	StatusPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<StatusPlugin, &StatusPlugin::handleRequest>("status");
	}

	~StatusPlugin()
	{
	}

private:
	virtual bool handleRequest(x0::HttpRequest *r, const x0::Params& args)
	{
		// set response status code
		r->status = x0::HttpError::Ok;
		r->responseHeaders.push_back("Content-Type", "text/html");

		r->write<x0::BufferSource>(createResponseBody());

		r->finish();

		return true;
	}

	// TODO let this method return a movable instead of a full copy
	x0::Buffer createResponseBody()
	{
		/*
		 * process uptime
		 * number of threads (total / active)
		 * cpu load (process / per worker)
		 * # requests
		 * # connections
		 * # requests since start
		 */

		x0::TimeSpan uptime(server().uptime());
		std::size_t nconns = 0;
		std::size_t nrequests = 0;
		unsigned long long numTotalRequests = 0;

		for (std::size_t i = 0, e = server().workers().size(); i != e; ++i) {
			const x0::HttpWorker *w = server().workers()[i];
			nconns += w->connectionLoad();
			nrequests += w->requestLoad();
			numTotalRequests += w->requestCount();
		}

		x0::Buffer buf;
		buf << "<html>";
		buf << "<head><title>x0 status page</title></head>\n";
		buf << "<body>";
		buf << "<h1>x0 status page</h1>\n";
		buf << "<pre>\n";
		buf << "process uptime: " << uptime << "\n";
		buf << "# workers: " << server().workers().size() << "\n";
		buf << "# requests: " << nrequests << "\n";
		buf << "# connections: " << nconns << "\n";
		buf << "# total requests: " << numTotalRequests << "\n";
		buf << "</pre>\n";
		buf << "</body></html>\n";

		return buf;
	}
};

X0_EXPORT_PLUGIN_CLASS(StatusPlugin)
