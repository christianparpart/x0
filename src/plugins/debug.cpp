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
#include <x0/types.hpp>

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

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class debug_plugin :
	public x0::plugin
{
private:
	x0::server::connection_hook::connection connection_open_;
	x0::server::request_parse_hook::connection pre_process_;
	x0::server::request_post_hook::connection request_done_;
	x0::server::connection_hook::connection connection_close_;

public:
	debug_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		connection_open_ = server_.connection_open.connect(boost::bind(&debug_plugin::connection_open, this, _1));
		pre_process_ = server_.pre_process.connect(boost::bind(&debug_plugin::pre_process, this, _1));
		request_done_ = server_.request_done.connect(boost::bind(&debug_plugin::request_done, this, _1, _2));
		connection_close_ = server_.connection_close.connect(boost::bind(&debug_plugin::connection_close, this, _1));
	}

	~debug_plugin() {
		server_.connection_open.disconnect(connection_open_);
		server_.pre_process.disconnect(pre_process_);
		server_.request_done.disconnect(request_done_);
		server_.connection_close.disconnect(connection_close_);
	}

	virtual void configure()
	{
	}

private:
	std::string client_hostname(x0::connection *connection)
	{
		std::string name = connection->remote_ip();

		if (name.empty())
			name = "<unknown>";

		name += ":";
		name += boost::lexical_cast<std::string>(connection->remote_port());

		return name;
	}

	struct timer :
		public x0::custom_data
	{
		ev::tstamp value_;
		struct ev_loop *loop_;

		explicit timer(struct ev_loop *loop) : value_(ev_now(loop)), loop_(loop) {}

		ev::tstamp diff() const { return ev_now(loop_) - value_; }
	};

	void connection_open(x0::connection *connection)
	{
		server_.log(x0::severity::info, "connection opened: %s", client_hostname(connection).c_str());
		connection->custom_data[this] = std::make_shared<timer>(server_.loop());
	}

	void pre_process(x0::request *in)
	{
		server_.log(x0::severity::info, "pre processing request from: %s", client_hostname(&in->connection).c_str());

		std::ostringstream stream;
		stream << "C> " << in->method.str() << ' ' << in->uri.str() << " HTTP/" << in->http_version_major << '.' << in->http_version_minor << std::endl;
		for (auto i = in->headers.begin(), e = in->headers.end(); i != e; ++i)
		{
			stream << "C> " << i->name.str() << ": " << i->value.str() << std::endl;
		}
		std::clog << stream.str();
	}

	void request_done(x0::request *in, x0::response *out)
	{
		//server_.log(x0::severity::info, "post process");

		std::ostringstream stream;

		if (!in->body.empty())
		{
			stream << "C> " << in->body << std::endl;
		}

		stream << "S< " << out->status() << ' ' << x0::response::status_str(out->status()) << std::endl;
		for (auto i = out->headers.begin(), e = out->headers.end(); i != e; ++i)
		{
			stream << "S< " << i->name << ": " << i->value << std::endl;
		}

		std::clog << stream.str();
	}

	void connection_close(x0::connection *connection)
	{
		if (std::shared_ptr<timer> tm = std::static_pointer_cast<timer>(connection->custom_data[this]))
		{
			server_.log(x0::severity::info, "connection timing: %.4f", tm->diff());
		}

		server_.log(x0::severity::info, "connection closed: %s", client_hostname(connection).c_str());
	}
};

X0_EXPORT_PLUGIN(debug);
