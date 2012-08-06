/* <x0/plugins/status.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
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

/**
 * \ingroup plugins
 * \brief example content generator plugin
 */
class StatusPlugin :
	public x0::HttpPlugin
{
private:
	std::list<x0::HttpConnection*> connections_;

public:
	StatusPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<StatusPlugin, &StatusPlugin::handleRequest>("status");
	}

private:
	virtual bool handleRequest(x0::HttpRequest *r, const x0::FlowParams& args)
	{
		// set response status code
		r->status = x0::HttpStatus::Ok;
		r->responseHeaders.push_back("Content-Type", "text/html; charset=utf-8");

		bool debug = true;

		r->write<x0::BufferSource>(createResponseBody(debug));

		r->finish();

		return true;
	}

	// TODO let this method return a movable instead of a full copy
	x0::Buffer createResponseBody(bool debug)
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
		double p1 = 0, p5 = 0, p15 = 0;

		for (std::size_t i = 0, e = server().workers().size(); i != e; ++i) {
			const x0::HttpWorker *w = server().workers()[i];
			nconns += w->connectionLoad();
			numTotalRequests += w->requestCount();
			numTotalConns += w->connectionCount();
			w->fetchPerformanceCounts(&p1, &p5, &p15);
		}

		x0::Buffer buf;
		buf << "<html>";
		buf << "<head><title>x0 status page</title>\n";
		buf <<
			"<style>"
			"#conn-table {"
				"border: 1px solid #ccc;"
				"font-size: 11px;"
			"}"
			"#conn-table th {"
				"border: 1px solid #ccc;"
				"padding-left: 4px;"
				"padding-right: 4px;"
			"}"
			"#conn-table td {"
				"border: 1px solid #ccc;"
				"padding-left: 4px;"
				"padding-right: 4px;"
				"white-space: nowrap;"
			"}"
			"td { vertical-align: top; }"
			".cid { text-align: right; }"
			".wid { text-align: right; }"
			".rn { text-align: right; }"
			".ip { text-align: center; }"
			".state { text-align: center; }"
			".age { text-align: right; }"
			".idle { text-align: right; }"
			".read { text-align: right; }"
			".written { text-align: right; }"
			".host { text-align: left; }"
			".method { text-align: center; }"
			".uri { text-align: left; }"
			".status { text-align: center; }"
			".debug { text-align: left; }"
			"</style>";
		buf << "</head>";
		buf << "<body>";
		buf << "<h1>x0 status page</h1>\n";
		buf << "<small><pre>" << server().tag() << "</pre></small>\n";
		buf << "<pre>\n";
		buf << "process uptime: " << uptime << "\n";
		buf << "process generation: " << server().generation() << "\n";

		buf << "average requests per second: ";
		char tmp[80];
		snprintf(tmp, sizeof(tmp), "%.2f, %.2f, %.2f", p1, p5, p15);
		buf << ((char*)tmp) << "\n";

		buf << "# workers: " << server().workers().size() << "\n";
		buf << "# connections: " << nconns << "\n";
		buf << "# total requests: " << numTotalRequests << "\n";
		buf << "# total connections: " << numTotalConns << "\n";
		buf << "</pre>\n";

		buf << "<table border='0' cellspacing='0' cellpadding='0' id='conn-table'>\n";

		buf << "<th>" << "cid" << "</th>";
		buf << "<th>" << "wid" << "</th>";
		buf << "<th>" << "r/n" << "</th>";
		buf << "<th>" << "IP" << "</th>";
		buf << "<th>" << "state" << "</th>";
		buf << "<th>" << "age" << "</th>";
		buf << "<th>" << "idle" << "</th>";
		buf << "<th>" << "read" << "</th>";
		buf << "<th>" << "written" << "</th>";
		buf << "<th>" << "host" << "</th>";
		buf << "<th>" << "method" << "</th>";
		buf << "<th>" << "uri" << "</th>";
		buf << "<th>" << "status" << "</th>";

		if (debug)
			buf << "<th>" << "debug" << "</th>";

		for (auto w: server().workers())
			for (auto c: w->connections())
				dump(buf, c, debug);

		buf << "</table>\n";

		buf << "</body></html>\n";

		return buf;
	}

	void dump(x0::Buffer& out, x0::HttpConnection* c, bool debug)
	{
		out << "<tr>";

		out << "<td class='cid'>" << c->id() << "</td>";
		out << "<td class='wid'>" << c->worker().id() << "</td>";
		out << "<td class='rn'>" << c->requestCount() << "</td>";
		out << "<td class='ip'>" << c->remoteIP() << "</td>";

		out << "<td class='state'>" << c->status_str();
		if (c->status() == x0::HttpConnection::ReadingRequest)
			out << " (" << c->state_str() << ")";
		out << "</td>";

		out << "<td class='age'>" << (c->worker().now() - c->socket()->startedAt()).str() << "</td>";
		out << "<td class='idle'>" << (c->worker().now() - c->socket()->lastActivityAt()).str() << "</td>";
		out << "<td class='read'>" << c->inputOffset() << "/" << c->inputSize() << "</td>";

		const x0::HttpRequest* r = c->request();
		if (r && c->status() != x0::HttpConnection::KeepAliveRead) {
			out << "<td class='written'>" << r->bytesTransmitted() << "</td>";
			out << "<td class='host'>" << sanitize(r->hostname) << "</td>";
			out << "<td class='method'>" << sanitize(r->method) << "</td>";
			out << "<td class='uri'>" << sanitize(r->uri) << "</td>";
			out << "<td class='status'>" << x0::make_error_code(r->status).message() << "</td>";
		} else {
			out << "<td colspan='5'>" << "</td>";
		}

		if (debug) {
			static const char *outputStateStr[] = {
				"unhandled",
				"populating",
				"finished",
			};
			out << "<td class='debug'>";
			out << "refcount:" << c->refCount() << ", ";
			out << "outputState:" << outputStateStr[c->request()->outputState()] << ", ";
			c->socket()->inspect(out);
			if (c->request()) {
				c->request()->inspect(out);
			}
			out << "</td>";
		}

		out << "</tr>\n";
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
					snprintf(buf, sizeof(buf), "&#%d;", static_cast<unsigned>(i) & 0xFF);
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
