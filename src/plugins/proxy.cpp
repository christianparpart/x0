/* <x0/plugins/proxy.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/plugin.hpp>
#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/web_client.hpp>
#include <x0/response_parser.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>

#include <cstring>
#include <cerrno>
#include <cstddef>
#include <deque>

#include <ev++.h>

#include <arpa/inet.h>		// inet_pton()
#include <netinet/tcp.h>	// TCP_QUICKACK, TCP_DEFER_ACCEPT
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

// TODO Implement proper error handling (origin connect or i/o errors, and client disconnect event).
// TODO Should we use getaddrinfo() instead of inet_pton()?
// TODO Implement proper timeout management for connect/write/read/keepalive timeouts.
// TODO Implement proper node load balancing.
// TODO Implement proper hot-spare fallback node activation, 

/* -- configuration proposal:
 *
 * Proxy = {
 *     Enabled = true;
 *     BufferSize = 0;
 *     ConnectTimeout = 5;
 *     IgnoreClientAbort = false;
 *     Mode = "reverse";                 -- "reverse" | "forward" | and possibly others
 *     KeepAlive = 0;                    -- keep-alive seconds to origin servers
 *     Methods = { 'PROPFIND' };
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

#if 0
#	define TRACE(msg...) /*!*/
#else
#	define TRACE(msg...) DEBUG("proxy: " msg)
#endif

class origin;
class proxy;
class proxy_connection;

class origin_server // {{{
{
private:
	sockaddr_in sa_;
	std::string hostname_;
	int port_;
	bool enabled_;
	std::string error_;

public:
	origin_server();
	origin_server(const std::string& hostname, int port);

	const std::string& hostname() const;
	int port();

	const sockaddr *address() const;
	int size() const;

	void enable();
	bool is_enabled() const;
	void disable();

	std::string error() const;
};
// }}}

// {{{ class proxy
/** holds a complete proxy configuration for a specific entry point.
 */
class proxy
{
public:
	struct ev_loop *loop;

public:
	// public configuration properties
	bool enabled;
	std::size_t buffer_size;
	std::size_t connect_timeout;
	std::size_t read_timeout;
	std::size_t write_timeout;
	std::size_t keepalive;
	bool ignore_client_abort;
	std::size_t keep_alive;
	std::vector<std::string> origins;
	std::vector<std::string> hot_spares;
	std::vector<std::string> allowed_methods;
	std::vector<origin_server> origins_;
	std::vector<std::string> ignores;

public:
	proxy();
	~proxy();

	bool method_allowed(const x0::buffer_ref& method) const;

	proxy_connection *acquire();
	void release(proxy_connection *px);

private:
	std::size_t origins_ptr;
	std::deque<proxy_connection *> idle_;
}; // }}}

class proxy_connection // {{{
{
	friend class proxy;


public:
	explicit proxy_connection(proxy *px);
	~proxy_connection();

	void start(const std::function<void()>& done, x0::request *in, x0::response *out);

private:
	void connect(const std::string& origin);
	void disconnect();

	void pass_request();

	void on_connect();
	void on_response(const x0::buffer_ref&, const x0::buffer_ref&, const x0::buffer_ref&);
	void on_header(const x0::buffer_ref&, const x0::buffer_ref&);
	void on_content(const x0::buffer_ref&);
	void on_complete();
	void content_written(int ec, std::size_t nb);

private:
	proxy *px_;								//!< owning proxy
	x0::web_client client_;					//!< HTTP client connection

	std::string hostname_;					//!< origin's hostname
	int port_;								//!< origin's port
	std::function<void()> done_;
	x0::request *request_;					//!< client's request
	x0::response *response_;				//!< client's response
	std::size_t serial_;
}; // }}}

// {{{ origin_server impl
inline origin_server::origin_server() :
	hostname_(),
	port_(),
	enabled_(false),
	error_()
{
	memset(&sa_, 0, sizeof(sa_));
}

inline origin_server::origin_server(const std::string& hostname, int port) :
	hostname_(hostname),
	port_(port),
	enabled_(true),
	error_()
{
	memset(&sa_, 0, sizeof(sa_));
	sa_.sin_family = AF_INET;
	sa_.sin_port = htons(port_);

	if (inet_pton(sa_.sin_family, hostname_.c_str(), &sa_.sin_addr) < 0)
	{
		error_ = strerror(errno);
		enabled_ = false;
	}
}

inline const std::string& origin_server::hostname() const
{
	return hostname_;
}

inline int origin_server::port()
{
	return port_;
}

inline const sockaddr *origin_server::address() const
{
	return reinterpret_cast<const sockaddr *>(&sa_);
}

inline int origin_server::size() const
{
	return sizeof(sa_);
}

inline void origin_server::enable()
{
	enabled_ = true;
}

inline bool origin_server::is_enabled() const
{
	return enabled_;
}

inline void origin_server::disable()
{
	enabled_ = false;
}

std::string origin_server::error() const
{
	return error_;
}
// }}}

// {{{ proxy impl
proxy::proxy() :
	loop(0),
	enabled(true),
	buffer_size(0),
	connect_timeout(8),
	read_timeout(0),
	write_timeout(8),
	keepalive(0),
	ignore_client_abort(false),
	keep_alive(/*10*/ 0),
	origins(),
	hot_spares(),
	allowed_methods(),
	origins_ptr(0)
{
	TRACE("proxy(%p) create", this);

	allowed_methods.push_back("GET");
	allowed_methods.push_back("HEAD");
	allowed_methods.push_back("POST");
}

proxy::~proxy()
{
	TRACE("proxy(%p) destroy", this);
}

bool proxy::method_allowed(const x0::buffer_ref& method) const
{
	if (x0::equals(method, "GET"))
		return true;

	if (x0::equals(method, "HEAD"))
		return true;

	if (x0::equals(method, "POST"))
		return true;

	for (std::vector<std::string>::const_iterator i = allowed_methods.begin(), e = allowed_methods.end(); i != e; ++i)
		if (x0::equals(method, *i))
			return true;

	return false;
}

proxy_connection *proxy::acquire()
{
	proxy_connection *px;
	if (!idle_.empty())
	{
		px = idle_.front();
		idle_.pop_front();
		TRACE("connection acquire.idle(%p)", px);
	}
	else
	{
		px = new proxy_connection(this);
		TRACE("connection acquire.new(%p)", px);

		px->connect(origins[origins_ptr]);

		if (++origins_ptr >= origins.size())
			origins_ptr = 0;
	}

	return px;
}

void proxy::release(proxy_connection *px)
{
	TRACE("connection release(%p)", px);
	idle_.push_back(px);
}
// }}}

// {{{ proxy_connection impl
proxy_connection::proxy_connection(proxy *px) :
	px_(px),
	client_(px_->loop),
	done_(),
	request_(NULL),
	response_(NULL),
	serial_(0)
{
	using namespace std::placeholders;

	client_.on_connect = std::bind(&proxy_connection::on_connect, this);
	client_.on_response = std::bind(&proxy_connection::on_response, this, _1, _2, _3);
	client_.on_header = std::bind(&proxy_connection::on_header, this, _1, _2);
	client_.on_content = std::bind(&proxy_connection::on_content, this, _1);
	client_.on_complete = std::bind(&proxy_connection::on_complete, this);
}

void proxy_connection::on_connect()
{
	if (!response_)
		return;

	pass_request();
}

void proxy_connection::on_response(const x0::buffer_ref& protocol, const x0::buffer_ref& code, const x0::buffer_ref& text)
{
	TRACE("connection(%p).on_status('%s', '%s', '%s')", this, protocol.str().c_str(), code.str().c_str(), text.str().c_str());
	response_->status = std::atoi(code.str().c_str());
}

inline bool validate_response_header(const x0::buffer_ref& name)
{
	if (iequals(name, "Connection"))
		return false;

	return true;
}

void proxy_connection::on_header(const x0::buffer_ref& name, const x0::buffer_ref& value)
{
	TRACE("connection(%p).on_header('%s', '%s')", this, name.str().c_str(), value.str().c_str());

	if (validate_response_header(name))
		response_->headers.set(name.str(), value.str());
}

void proxy_connection::on_content(const x0::buffer_ref& value)
{
	TRACE("connection(%p).on_content(size=%ld)", this, value.size());

	client_.pause();

	response_->write(std::make_shared<x0::buffer_source>(value),
			std::bind(&proxy_connection::content_written, this, std::placeholders::_1, std::placeholders::_2));
}

void proxy_connection::on_complete()
{
}

void proxy_connection::content_written(int ec, std::size_t nb)
{
	TRACE("connection(%p).content_written(ec=%d, nb=%ld): %s", this, ec, nb, ec ? strerror(errno) : "");

	if (!ec)
	{
		client_.resume();
	}
	else
	{
		ec = errno;
		request_->connection.server().log(x0::severity::notice, "proxy: client %s aborted with %s.",
				request_->connection.remote_ip().c_str(), strerror(ec));

		delete this;
	}
}

proxy_connection::~proxy_connection()
{
	TRACE("~proxy_connection(%p) destroy", this);
}

/** Asynchronously connects to origin server.
 */
void proxy_connection::connect(const std::string& origin)
{
	std::string protocol;
	if (!x0::parse_url(origin, protocol, hostname_, port_))
	{
		TRACE("proxy_connection(%p).connect() failed: %s.", this, "Origin URL parse error");
		return;
	}

	TRACE("proxy_connection(%p): connecting to %s port %d", this, hostname_.c_str(), port_);

	client_.open(hostname_, port_);

	switch (client_.state())
	{
		case x0::web_client::DISCONNECTED:
			TRACE("proxy_connection(%p): connect error: %s", this, client_.message().c_str());
			break;
		default:
			break;
	}
}

/** Disconnects from origin server, possibly finishing client response aswell.
 */
void proxy_connection::disconnect()
{
	client_.close();
}

/** Starts processing client request.
 *
 * \param done Callback to invoke when request has been fully processed (or an error occurred).
 * \param in Corresponding request.
 * \param out Corresponding response.
 */
void proxy_connection::start(const std::function<void()>& done, x0::request *in, x0::response *out)
{
	TRACE("connection(%p).start(): path=%s (client_.state()=%d)", this, in->path.str().c_str(), client_.state());

	if (client_.state() != x0::web_client::DISCONNECTED)
	{
		done_ = done;
		request_ = in;
		response_ = out;
	}
	else
	{
		out->status = x0::response::service_unavailable;
		done();
		delete this;
		return;
	}
}

inline bool validate_request_header(const x0::buffer_ref& name)
{
	if (x0::iequals(name, "Host"))
		return false;

	if (x0::iequals(name, "Keep-Alive"))
		return false;

	return true;
}

void proxy_connection::pass_request()
{
	TRACE("connection(%p).pass_request()", this);

	// request line
	if (request_->query)
		client_.pass_request(request_->method, request_->path, request_->query);
	else
		client_.pass_request(request_->method, request_->path);

	// request-headers
	for (auto i = request_->headers.begin(), e = request_->headers.end(); i != e; ++i)
		if (validate_request_header(i->name))
			client_.pass_header(i->name, i->value);

	{
		x0::buffer result;

		auto hostid = request_->hostid();
		std::size_t n = hostid.find(':');
		if (n != std::string::npos)
			result.push_back(hostid.substr(0, n));
		else
			result.push_back(hostid);

		result.push_back(':');
		result.push_back(port_);

		client_.pass_header("Host", result);
	}

	// body?
	if (request_->body.empty())
	{
		;//! \todo properly handle POST data (request_->body)
	}

	client_.commit(true);
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

public:
	proxy_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		using namespace std::placeholders;

		// register content generator
		c = server_.generate_content.connect(&proxy_plugin::process, this);

		server_.register_cvar_path("Proxy", std::bind(&proxy_plugin::setup_proxy, this, _1, _2, _3));
	}

	void setup_proxy(const x0::settings_value& cvar, const std::string& hostid, const std::string& path)
	{
		server_.log(x0::severity::debug, "setup_proxy(host=%s, path=%s)", hostid.c_str(), path.c_str());
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
			server_.config()["Hosts"][*i]["Proxy"]["ReadTimeout"].load(px->read_timeout);
			server_.config()["Hosts"][*i]["Proxy"]["WriteTimeout"].load(px->write_timeout);
			server_.config()["Hosts"][*i]["Proxy"]["KeepAlive"].load(px->keepalive);
			server_.config()["Hosts"][*i]["Proxy"]["BufferSize"].load(px->buffer_size);
			server_.config()["Hosts"][*i]["Proxy"]["Methods"].load(px->allowed_methods);

			server_.config()["Hosts"][*i]["Proxy"]["Origins"].load(px->origins);

			for (std::size_t k = 0; k < px->origins.size(); ++k)
			{
				std::string url = px->origins[k];
				std::string protocol, hostname;
				int port = 0;

				if (!x0::parse_url(url, protocol, hostname, port))
				{
					TRACE("%s.", "Origin URL parse error");
					continue;
				}

				origin_server origin(hostname, port);
				if (origin.is_enabled())
					px->origins_.push_back(origin);
				else
					server_.log(x0::severity::error, origin.error().c_str());
			}

			if (px->origins_.empty())
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

		if (!px->method_allowed(in->method))
		{
			TRACE("method not allowed");
			return next();
		}

		proxy_connection *connection = px->acquire();
		connection->start(std::bind(&proxy_plugin::done, this, next), in, out);
	}

	void done(x0::request_handler::invokation_iterator next)
	{
		TRACE("done processing request");

		// we're done processing this request
		// -> make room for possible more requests to be processed by this connection
		next.done();
	}
};

X0_EXPORT_PLUGIN(proxy);
