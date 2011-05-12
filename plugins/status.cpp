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
#include <x0/http/HttpConnection.h>
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

class ConnectionHook :
	public x0::CustomDataMgr
{
private:
	ev::tstamp createdAt_;

public:
};

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class StatusPlugin :
	public x0::HttpPlugin
{
private:
	std::list<x0::HttpConnection*> connections_;

	void onConnectionCreate(x0::HttpConnection* c)
	{
	}

	void onConnectionDestroy(x0::HttpConnection* c)
	{
	}

public:
	StatusPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<StatusPlugin, &StatusPlugin::handleRequest>("status");

		srv.onConnectionOpen.connect<StatusPlugin, &StatusPlugin::onConnectionCreate>(this);
		srv.onConnectionClose.connect<StatusPlugin, &StatusPlugin::onConnectionCreate>(this);
	}

	~StatusPlugin()
	{
		server().onConnectionOpen.disconnect(this);
		server().onConnectionClose.disconnect(this);
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
		unsigned long long numTotalRequests = 0;
		unsigned long long numTotalConns = 0;

		for (std::size_t i = 0, e = server().workers().size(); i != e; ++i) {
			const x0::HttpWorker *w = server().workers()[i];
			nconns += w->connectionLoad();
			numTotalRequests += w->requestCount();
			numTotalConns += w->connectionCount();
		}

		x0::Buffer buf;
		buf << "<html>";
		buf << "<head><title>x0 status page</title></head>\n";
		buf << "<body>";
		buf << "<h1>x0 status page</h1>\n";
		buf << "<pre>\n";
		buf << "process uptime: " << uptime << "\n";
		buf << "# workers: " << server().workers().size() << "\n";
		buf << "# connections: " << nconns << "\n";
		buf << "# total requests: " << numTotalRequests << "\n";
		buf << "# total connections: " << numTotalConns << "\n";
		buf << "\n";

		for (auto w: server().workers())
			for (auto c: w->connections())
				dump(buf, c);

		buf << "</pre>\n";
		buf << "</body></html>\n";

		return buf;
	}

	void dump(x0::Buffer& out, x0::HttpConnection* c)
	{
		out << c->worker().id() << "." << c->id() << ", ";
		out << c->state_str() << ", " << c->status_str() << ": ";

		const x0::HttpRequest* r = c->request();
		if (r && c->status() != x0::HttpConnection::KeepAliveRead) {
			out << '@' << sanitize(r->hostname) << ": " << sanitize(r->method) << ' ' << sanitize(r->uri)
				<< ' ' << x0::make_error_code(r->status).message()
				<< ' ' << r->bytesTransmitted()
				<< "\n";
		} else
			out << "\n";

		out << "    ";
		c->socket()->inspect(out);
	}

	template<typename T>
	std::string sanitize(const T& value)
	{
		std::string out;
		char buf[32];
		for (auto i: value) {
			switch (i) {
				case '<':
				case '>':
				case '&':
					snprintf(buf, sizeof(buf), "#%d;", static_cast<unsigned>(i) & 0xFF);
					out += buf;
					break;
				default:
					out += i;
					break;
			}
		}
		return !out.empty() ? out : "(null)";
	}
};

X0_EXPORT_PLUGIN_CLASS(StatusPlugin)
