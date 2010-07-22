/* <x0/plugins/debug.cpp>
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
#include <x0/http/HttpRangeDef.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>

#include <sstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ev++.h>

struct cstat : // {{{
	public x0::CustomData
{
private:
	static unsigned connection_counter;

	x0::HttpServer& server_;
	ev::tstamp start_;
	unsigned cid_;
	unsigned rcount_;
	FILE *fp;

private:
	template<typename... Args>
	void log(x0::Severity s, const char *fmt, Args&& ... args)
	{
		//server_.log(s, fmt, args...);

		if (fp)
		{
			fprintf(fp, "%.4f ", ev_now(server_.loop()));
			fprintf(fp, fmt, args...);
			fprintf(fp, "\n");
			fflush(fp);
		}
	}

public:
	explicit cstat(x0::HttpServer& server) :
		server_(server),
		start_(ev_now(server.loop())),
		cid_(++connection_counter),
		rcount_(0),
		fp(NULL)
	{

		char buf[1024];
		snprintf(buf, sizeof(buf), "c%04d.log", id());
		fp = fopen(buf, "w");

		log(x0::Severity::info, "connection[%d] opened.", id());
	}

	ev::tstamp connection_time() const { return ev_now(server_.loop()) - start_; }
	unsigned id() const { return cid_; }
	unsigned request_count() const { return rcount_; }

	void begin_request(x0::HttpRequest *in)
	{
		++rcount_;

		log(x0::Severity::info, "connection[%d] request[%d]: %s %s",
				id(), request_count(), in->method.str().c_str(), in->uri.str().c_str());

		for (auto i = in->headers.begin(), e = in->headers.end(); i != e; ++i)
			log(x0::Severity::info, "C> %s: %s", i->name.str().c_str(), i->value.str().c_str());
	}

	void end_request(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		//std::ostringstream stream;

		/*if (!in->body.empty())
		{
			log(x0::Severity::info, "C> %s", in->body.c_str());
		}*/

		//stream << "S< " << static_cast<int>(out->status) << ' ' << x0::HttpResponse::status_str(out->status) << std::endl;
		for (auto i = out->headers.begin(), e = out->headers.end(); i != e; ++i)
			log(x0::Severity::info, "S< %s: %s", i->name.c_str(), i->value.c_str());
	}

	~cstat()
	{
		log(x0::Severity::info, "connection[%d] closed. timing: %.4f (nreqs: %d)",
				id(), connection_time(), request_count());

		if (fp)
			fclose(fp);
	}
};

unsigned cstat::connection_counter = 0;
// }}}

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class debug_plugin :
	public x0::HttpPlugin
{
private:
	x0::HttpServer::ConnectionHook::Connection connection_open_;
	x0::HttpServer::RequestHook::Connection pre_process_;
	x0::HttpServer::RequestPostHook::Connection request_done_;
	x0::HttpServer::ConnectionHook::Connection connection_close_;

public:
	debug_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		connection_open_ = server_.onConnectionOpen.connect<debug_plugin, &debug_plugin::connection_open>(this);
		pre_process_ = server_.onPreProcess.connect<debug_plugin, &debug_plugin::pre_process>(this);
		request_done_ = server_.onRequestDone.connect<debug_plugin, &debug_plugin::request_done>(this);
		connection_close_ = server_.onConnectionClose.connect<debug_plugin, &debug_plugin::connection_close>(this);
	}

	~debug_plugin() {
		server_.onConnectionOpen.disconnect(connection_open_);
		server_.onPreProcess.disconnect(pre_process_);
		server_.onRequestDone.disconnect(request_done_);
		server_.onConnectionClose.disconnect(connection_close_);
	}

private:
	std::string client_hostname(x0::HttpConnection *connection)
	{
		std::string name = connection->remote_ip();

		if (name.empty())
			name = "<unknown>";

		name += ":";
		name += boost::lexical_cast<std::string>(connection->remote_port());

		return name;
	}

	void connection_open(x0::HttpConnection *connection)
	{
		connection->custom_data[this] = std::make_shared<cstat>(server_);
	}

	void pre_process(x0::HttpRequest *in)
	{
		if (std::shared_ptr<cstat> cs = std::static_pointer_cast<cstat>(in->connection.custom_data[this]))
			cs->begin_request(in);
	}

	void request_done(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (std::shared_ptr<cstat> cs = std::static_pointer_cast<cstat>(in->connection.custom_data[this]))
			cs->end_request(in, out);
	}

	void connection_close(x0::HttpConnection *connection)
	{
		// we don't need this at the moment
	}
};

X0_EXPORT_PLUGIN(debug);
