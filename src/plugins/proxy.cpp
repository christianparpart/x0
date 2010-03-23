/* <x0/plugins/proxy.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/response_parser.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <ev++.h>

#include <arpa/inet.h>		// inet_pton()
#include <netinet/tcp.h>	// TCP_QUICKACK, TCP_DEFER_ACCEPT
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

/* -- configuration proposal:
 *
 * Proxy = {
 *     Enabled = true;
 *     BufferSize = 0;
 *     ConnectTimeout = 5;
 *     IgnoreClientAbort = false;
 *     Mode = "reverse";                 -- "reverse" | "forward" | and possibly others
 *     KeepAlive = 0;                    -- keep-alive seconds to origin servers
 *     Servers = {
 *         "http://pr1.backend/"
 *     };
 *     HotSpares = {
 *         "http://hs1.backend.net/"
 *     };
 * };
 *
 * Allowed Contexts: vhost, location
 *
 */

#define TRACE(msg...) DEBUG("proxy: " msg)

class proxy;
class proxy_connection;

/** holds a complete proxy configuration for a specific entry point.
 */
class proxy
{
public:
	struct ev_loop *loop;
	bool enabled;
	std::size_t buffer_size;
	std::size_t connect_timeout;
	bool ignore_client_abort;
	std::size_t keep_alive;
	std::vector<std::string> origins;
	std::vector<std::string> hot_spares;

	proxy();
	~proxy();

	proxy_connection *acquire();
	void release(proxy_connection *px);

private:
	std::size_t origins_ptr;
};

class proxy_connection
{
	friend class proxy;

public:
	enum state {
		not_connected,		//!< not connected to origin
		connecting,			//!< connect() in progress
		connected,			//!< connected to origin but idle
		processing			//!< connected to origin and processing for/to a client
	};

public:
	explicit proxy_connection(proxy *px);
	~proxy_connection();

	void start(const std::function<void()>& done, x0::request *in, x0::response *out);
	bool active() const;
	void stop();

private:
	void connect(const std::string& origin);
	void disconnect();

	void pass_request();

	void on_status(const x0::buffer_ref&, const x0::buffer_ref&, const x0::buffer_ref&);
	void on_header(const x0::buffer_ref&, const x0::buffer_ref&);
	void on_content(const x0::buffer_ref&);
	void content_written(int ec, std::size_t nb);

	void io(ev::io& w, int revents);
	void timeout(ev::timer& w, int revents);

private:
	state state_;
	proxy *px_;
	int origin_;

	std::function<void()> done_;
	x0::request *request_;
	x0::response *response_;
	x0::response_parser response_parser_;

	x0::buffer write_buffer_;
	std::size_t write_offset_;

	x0::buffer read_buffer_;
	std::size_t serial_;

	ev::io io_;
	ev::timer timer_;
};

// {{{ proxy impl
proxy::proxy() :
	loop(0),
	enabled(true),
	buffer_size(8192),
	connect_timeout(8),
	ignore_client_abort(false),
	keep_alive(10),
	origins(),
	hot_spares(),
	origins_ptr(0)
{
}

proxy::~proxy()
{
}

proxy_connection *proxy::acquire()
{
	proxy_connection *px = new proxy_connection(this);

	px->connect(origins[origins_ptr]);

	if (origins_ptr < origins.size())
		origins_ptr++;
	else
		origins_ptr = 0;

	return px;
}

void proxy::release(proxy_connection *px)
{
	delete px;
}
// }}}

// {{{ proxy_connection impl
proxy_connection::proxy_connection(proxy *px) :
	state_(not_connected),
	px_(px),
	origin_(-1),
	done_(),
	request_(NULL),
	response_(NULL),
	response_parser_(x0::response_parser::ALL),
	write_buffer_(),
	write_offset_(0),
	read_buffer_(),
	serial_(0),
	io_(px_->loop),
	timer_(px_->loop)
{
	io_.set<proxy_connection, &proxy_connection::io>(this);
	timer_.set<proxy_connection, &proxy_connection::timeout>(this);

	using namespace std::placeholders;
	response_parser_.status = std::bind(&proxy_connection::on_status, this, _1, _2, _3);
	response_parser_.assign_header = std::bind(&proxy_connection::on_header, this, _1, _2);
	response_parser_.process_content = std::bind(&proxy_connection::on_content, this, _1);
}

void proxy_connection::on_status(const x0::buffer_ref& protocol, const x0::buffer_ref& code, const x0::buffer_ref& text)
{
	TRACE("on_status('%s', '%s', '%s')", protocol.str().c_str(), code.str().c_str(), text.str().c_str());
	response_->status = std::atoi(code.str().c_str());
}

void proxy_connection::on_header(const x0::buffer_ref& name, const x0::buffer_ref& value)
{
	TRACE("on_header('%s', '%s')", name.str().c_str(), value.str().c_str());
	response_->headers.set(name.str(), value.str());
}

void proxy_connection::on_content(const x0::buffer_ref& value)
{
	TRACE("on_content(size=%ld)", value.size());

	io_.stop();
	response_->write(std::make_shared<x0::buffer_source>(value),
			std::bind(&proxy_connection::content_written, this, std::placeholders::_1, std::placeholders::_2));
}

void proxy_connection::content_written(int ec, std::size_t nb)
{
	io_.start(origin_, ev::READ);
}

proxy_connection::~proxy_connection()
{
}

/** Asynchronously connects to origin server.
 */
void proxy_connection::connect(const std::string& origin)
{
	std::string protocol, hostname;
	int port = 0;
	if (!x0::parse_url(origin, protocol, hostname, port))
	{
		TRACE("connect() failed: %s.", "Origin URL parse error");
		return;
	}

	TRACE("connecting to %s port %d", hostname.c_str(), port);

	origin_ = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (origin_ == -1)
	{
		TRACE("socket() failed: %s.", strerror(errno));
		return;
	}

	int rv = fcntl(origin_, F_GETFL, NULL) | O_NONBLOCK | O_CLOEXEC;
	fcntl(origin_, F_SETFL, &rv, sizeof(rv));

	sockaddr_in6 sin;
	bzero(&sin, sizeof(sin));

	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons(port);
	if (inet_pton(sin.sin6_family, hostname.c_str(), sin.sin6_addr.s6_addr) < 0)
	{
		TRACE("async_connect: resolve error: %s", strerror(errno));
		return;
	}

	socklen_t salen = sizeof(sockaddr_in6);

	rv = ::connect(origin_, (sockaddr *)&sin, salen);

	if (rv == -1)
	{
		if (errno != EINPROGRESS)
		{
			TRACE("async_connect error: %s", strerror(errno));
			::close(origin_);
			origin_ = -1;
			return;
		}
		TRACE("async_connect: backgrounding");

		io_.start(origin_, ev::WRITE);

		if (px_->connect_timeout != 0)
			timer_.start();

		state_ = connecting;
	}
	else
	{
		state_ = connected;
		TRACE("async_connect: instant success");

		if (request_)
			pass_request();
	}
}

/** Disconnects from origin server, possibly finishing client response aswell.
 */
void proxy_connection::disconnect()
{
	if (origin_ != -1)
	{
		::close(origin_);
		origin_ = -1;
	}
}

/** Starts processing client request.
 *
 * \param done Callback to invoke when request has been fully processed (or an error occurred).
 * \param in Corresponding request.
 * \param out Corresponding response.
 */
void proxy_connection::start(const std::function<void()>& done, x0::request *in, x0::response *out)
{
	done_ = done;
	request_ = in;
	response_ = out;

	if (state_ == connected)
		pass_request();
}

void proxy_connection::pass_request()
{
	TRACE("pass_request()");

	write_buffer_.clear();
	write_offset_ = 0;

	// request line
	write_buffer_.push_back(request_->method);
	write_buffer_.push_back(' ');
	write_buffer_.push_back(request_->path);
	if (request_->query)
	{
		write_buffer_.push_back('?');
		write_buffer_.push_back(request_->query);
	}
	write_buffer_.push_back(" HTTP/1.0\r\n");

	// headers
	for (auto i = request_->headers.begin(), e = request_->headers.end(); i != e; ++i)
	{
		if (x0::iequals(i->name, "Host"))
			continue;

		write_buffer_.push_back(i->name);
		write_buffer_.push_back(": ");
		write_buffer_.push_back(i->value);
		write_buffer_.push_back("\r\n");
	}
	write_buffer_.push_back("Host: localhost\r\n");
	write_buffer_.push_back("\r\n");

	// body?
	if (request_->body.empty())
		write_buffer_.push_back(request_->body);

	state_ = processing;

	io_.start(origin_, ev::WRITE);
	timer_.start(16.0, 0.0);
}

/** Explicitely stops processing active client request.
 */
void proxy_connection::stop()
{
	request_ = NULL;
	response_ = NULL;

	io_.stop();
	done_();

	done_ = std::function<void()>();
}

void proxy_connection::io(ev::io& w, int revents)
{
	if (revents & ev::READ)
	{
		TRACE("connection::io(%d, ev::READ)", w.fd);
		std::size_t lower_bound = read_buffer_.size();

		if (lower_bound == read_buffer_.capacity())
			read_buffer_.capacity(lower_bound + 4096);

		size_t rv = ::read(origin_, (char *)read_buffer_.end(), read_buffer_.capacity() - lower_bound);
		if (rv > 0)
		{
			TRACE("read response: %ld bytes", rv);
			read_buffer_.resize(lower_bound + rv);
			response_parser_.parse(read_buffer_.ref(lower_bound, rv));
			serial_++;
		}
		else if (rv == 0)
		{
			stop();
		}
		else
		{
			TRACE("read response failed(%ld): %s", rv, strerror(errno));
			stop();
		}
	}

	if (revents & ev::WRITE)
	{
		TRACE("connection::io(%d, ev::WRITE)", w.fd);
		if (state_ == connecting)
		{
			int val = 0;
			socklen_t vlen = sizeof(val);
			if (getsockopt(origin_, SOL_SOCKET, SO_ERROR, &val, &vlen) == 0)
			{
				if (val == 0)
				{
					TRACE("async_connect: connected");
					state_ = connected;
					if (request_)
						pass_request();
				}
				else
				{
					TRACE("async_connect: error(%d): %s", val, strerror(val));
				}
			}
		}
		else
		{
			size_t rv = ::write(origin_, write_buffer_.data() + write_offset_, write_buffer_.size() - write_offset_);
			if (rv > 0)
			{
				TRACE("write request: %ld (of %ld) bytes", rv, write_buffer_.size() - write_offset_);

				write_offset_ += rv;
				if (write_offset_ == write_buffer_.size()) // request fully transmitted, let's read response then.
				{
					io_.stop();
					io_.start(origin_, ev::READ);
					serial_ = 0;
				}
			}
			else
			{
				TRACE("write request failed(%ld): %s", rv, strerror(errno));
				io_.stop();
			}
		}
	}
}

void proxy_connection::timeout(ev::timer& w, int revents)
{
	switch (state_)
	{
		case connecting:
			break;
		case processing:
			break;
		case not_connected:
		case connected:
			// unexpected invokation
			break;
	}
}
// }}}

/**
 * \ingroup plugins
 * \brief proxy content generator plugin
 */
class proxy_plugin :
	public x0::plugin
{
private:
	x0::request_handler::connection c;

	struct context
	{
		bool enabled;
		std::string prefix;
	};

public:
	proxy_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		// register content generator
		c = server_.generate_content.connect(&proxy_plugin::process, this);
	}

	~proxy_plugin()
	{
		c.disconnect(); // optional, as it gets invoked on ~connection(), too.
	}

	virtual void configure()
	{
		auto hosts = server_.config()["Hosts"].keys<std::string>();
		for (auto i = hosts.begin(), e = hosts.end(); i != e; ++i)
		{
			if (!server_.config()["Hosts"][*i].contains("Proxy"))
				continue; // no proxy

			proxy *px = server_.create_context<proxy>(this, *i);

			px->loop = server_.loop();

			server_.config()["Hosts"][*i]["Proxy"]["Enabled"].load(px->enabled);
			server_.config()["Hosts"][*i]["Proxy"]["ConnectTimeout"].load(px->connect_timeout);
			server_.config()["Hosts"][*i]["Proxy"]["BufferSize"].load(px->buffer_size);
			server_.config()["Hosts"][*i]["Proxy"]["Origins"].load(px->origins);
			server_.config()["Hosts"][*i]["Proxy"]["HotSpares"].load(px->hot_spares);

			if (px->origins.empty())
				server_.log(x0::severity::warn, "No origin servers defined for proxy at virtual-host: %s.", i->c_str());
		}
	}

private:
	void process(x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out)
	{
		proxy *px = server_.context<proxy>(this, in->hostid());
		if (!px)
			return next();

		if (!px->enabled)
			return next();

		proxy_connection *connection = px->acquire();
		connection->start(next, in, out);
	}

	void done(x0::request_handler::invokation_iterator next)
	{
		// we're done processing this request
		// -> make room for possible more requests to be processed by this connection
		next.done();
	}
};

X0_EXPORT_PLUGIN(proxy);
