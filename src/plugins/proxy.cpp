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
#include <x0/io/buffer_source.hpp>
#include <x0/strutils.hpp>
#include <x0/url.hpp>
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

// CODE CLEANUPS:
// - TODO Create proxy_connection at pre_process to connect to backend and pass
// 			possible request content already
// - TODO Implement proper error handling (origin connect or i/o errors,
// 			and client disconnect event).
// - TODO Should we use getaddrinfo() instead of inet_pton()?
// - TODO Implement proper timeout management for connect/write/read/keepalive
// 			timeouts.
// FEATURES:
// - TODO Implement proper node load balancing.
// - TODO Implement proper hot-spare fallback node activation, 

/* -- configuration proposal:
 *
 * ['YourDomain.com'] = {
 *     ProxyEnabled = true;
 *     ProxyBufferSize = 0;
 *     ProxyConnectTimeout = 5;
 *     ProxyIgnoreClientAbort = false;
 *     ProxyMode = "reverse";                 -- "reverse" | "forward" | and possibly others
 *     ProxyKeepAlive = 0;                    -- keep-alive seconds to origin servers
 *     ProxyMethods = { 'PROPFIND' };
 *     ProxyServers = {
 *         "http://pr1.backend/"
 *     };
 *     ProxyHotSpares = {
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

// {{{ class proxy_connection
/** handles a connection from proxy to origin server.
 */
class proxy_connection :
	public x0::web_client_base
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
	void pass_request_content(x0::buffer_ref&& chunk);

	virtual void connect();
	virtual void response(int, int, int, x0::buffer_ref&&);
	virtual void header(x0::buffer_ref&&, x0::buffer_ref&&);
	virtual bool content(x0::buffer_ref&&);
	virtual bool complete();

	void content_written(int ec, std::size_t nb);

private:
	proxy *px_;								//!< owning proxy

	std::string hostname_;					//!< origin's hostname
	int port_;								//!< origin's port
	std::function<void()> done_;
	x0::request *request_;					//!< client's request
	x0::response *response_;				//!< client's response
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
	web_client_base(px->loop),
	px_(px),
	done_(),
	request_(NULL),
	response_(NULL)
{
}

/** callback, invoked once the connection to the origin server is established.
 *
 * Though, passing the request message to the origin server.
 */
void proxy_connection::connect()
{
	TRACE("connection(%p).connect()", this);
	if (!response_)
		return;

	pass_request();
}

/** callback, invoked when the origin server has passed us the response status line.
 *
 * We will use the status code only.
 * However, we could pass the text field, too - once x0 core supports it.
 */
void proxy_connection::response(int major, int minor, int code, x0::buffer_ref&& text)
{
	TRACE("proxy_connection(%p).status(HTTP/%d.%d, %d, '%s')", this, major, minor, code, text.str().c_str());
	response_->status = static_cast<x0::http_error>(code);
}

inline bool validate_response_header(const x0::buffer_ref& name)
{
	// XXX do not allow origin's connection-level response headers to be passed to the client.
	if (iequals(name, "Connection"))
		return false;

	if (iequals(name, "Transfer-Encoding"))
		return false;

	return true;
}

/** callback, invoked on every successfully parsed response header.
 *
 * We will pass this header directly to the client's response if
 * that is NOT a connection-level header.
 */
void proxy_connection::header(x0::buffer_ref&& name, x0::buffer_ref&& value)
{
	TRACE("proxy_connection(%p).header('%s', '%s')", this, name.str().c_str(), value.str().c_str());

	if (validate_response_header(name))
		response_->headers.set(name.str(), value.str());
}

/** callback, invoked when a new content chunk from origin has arrived.
 *
 * We temporarily pause the client to not receive any more data
 * until having fully transmitted the currently passed one to the actual client.
 *
 * The client must be resumed once the current chunk has been fully passed
 * to the client.
 */
bool proxy_connection::content(x0::buffer_ref&& chunk)
{
	TRACE("proxy_connection(%p).content(size=%ld)", this, chunk.size());

	pause();

	response_->write(std::make_shared<x0::buffer_source>(chunk),
			std::bind(&proxy_connection::content_written, this, std::placeholders::_1, std::placeholders::_2));

	return true;
}

/** callback, invoked once the origin's response message has been fully received.
 *
 * We will inform x0 core, that we've finished processing this request and destruct ourselfs.
 */
bool proxy_connection::complete()
{
	TRACE("proxy_connection(%p).complete()", this);

	if (static_cast<int>(response_->status) == 0)
		response_->status = x0::http_error::service_unavailable;

	done_();
	delete this;

	// do not continue processing
	return false;
}

/** completion handler, invoked when response content chunk has been sent to the client.
 *
 * If the previousely transferred chunk has been successfully written, we will
 * resume receiving response content from the origin server, or kill us otherwise.
 */
void proxy_connection::content_written(int ec, std::size_t nb)
{
	TRACE("connection(%p).content_written(ec=%d, nb=%ld): %s", this, ec, nb, ec ? strerror(errno) : "");

	if (!ec)
	{
		resume();
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
 *
 * \param origin the origin's HTTP URL.
 *
 * \note this can also result into a non-async connect if it would not block.
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

	open(hostname_, port_);

	switch (state())
	{
		case DISCONNECTED:
			TRACE("proxy_connection(%p): connect error: %s", this, last_error().message().c_str());
			break;
		default:
			break;
	}
}

/** Disconnects from origin server, possibly finishing client response aswell.
 */
void proxy_connection::disconnect()
{
	close();
}

/** Starts processing the client request.
 *
 * \param done Callback to invoke when request has been fully processed (or an error occurred).
 * \param in Corresponding request.
 * \param out Corresponding response.
 */
void proxy_connection::start(const std::function<void()>& done, x0::request *in, x0::response *out)
{
	TRACE("connection(%p).start(): path=%s (state()=%d)", this, in->path.str().c_str(), state());

	if (state() == DISCONNECTED)
	{
		out->status = x0::http_error::service_unavailable;
		done();
		delete this;
		return;
	}

	done_ = done;
	request_ = in;
	response_ = out;

	if (state() == CONNECTED)
		pass_request();
}

/** test whether or not this request header may be passed to the origin server.
 */
inline bool validate_request_header(const x0::buffer_ref& name)
{
	if (x0::iequals(name, "Host"))
		return false;

	if (x0::iequals(name, "Accept-Encoding"))
		return false;

	if (x0::iequals(name, "Connection"))
		return false;

	if (x0::iequals(name, "Keep-Alive"))
		return false;

	return true;
}

/** startss passing the client request message to the origin server.
 */
void proxy_connection::pass_request()
{
	TRACE("connection(%p).pass_request('%s', '%s', '%s')", this, 
		request_->method.str().c_str(), request_->path.str().c_str(), request_->query.str().c_str());

	// request line
	if (request_->query)
		write_request(request_->method, request_->path, request_->query);
	else
		write_request(request_->method, request_->path);

	// request-headers
	for (auto i = request_->headers.begin(), e = request_->headers.end(); i != e; ++i)
		if (validate_request_header(i->name))
			write_header(i->name, i->value);

	if (!hostname_.empty())
	{
		write_header("Host", hostname_);
	}
	else
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

		write_header("Host", result);
	}

	//! \todo body?
#if 0
	if (request_->body.empty())
	{
		;//! \todo properly handle POST data (request_->body)
	}
#endif

	commit(true);

	if (request_->content_available())
	{
		using namespace std::placeholders;
		request_->read(std::bind(&proxy_connection::pass_request_content, this, _1));
	}
}

/** callback, invoked when client content chunk is available, and is to pass to the origin server.
 */
void proxy_connection::pass_request_content(x0::buffer_ref&& chunk)
{
	TRACE("proxy_connection.pass_request_content(): '%s'", chunk.str().c_str());
	//client.write(chunk, std::bind(&proxy_connection::request_content_passed, this);
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

		server_.register_cvar_host("ProxyEnable", std::bind(&proxy_plugin::setup_proxy_enable, this, _1, _2));
		server_.register_cvar_host("ProxyMode", std::bind(&proxy_plugin::setup_proxy_mode, this, _1, _2));
		server_.register_cvar_host("ProxyOrigins", std::bind(&proxy_plugin::setup_proxy_origins, this, _1, _2));
		server_.register_cvar_host("ProxyHotSpares", std::bind(&proxy_plugin::setup_proxy_hotspares, this, _1, _2));
		server_.register_cvar_host("ProxyMethods", std::bind(&proxy_plugin::setup_proxy_methods, this, _1, _2));
		server_.register_cvar_host("ProxyConnectTimeout", std::bind(&proxy_plugin::setup_proxy_connect_timeout, this, _1, _2));
		server_.register_cvar_host("ProxyReadTimeout", std::bind(&proxy_plugin::setup_proxy_read_timeout, this, _1, _2));
		server_.register_cvar_host("ProxyWriteTimeout", std::bind(&proxy_plugin::setup_proxy_write_timeout, this, _1, _2));
		server_.register_cvar_host("ProxyKeepAliveTimeout", std::bind(&proxy_plugin::setup_proxy_keepalive_timeout, this, _1, _2));
	}

	~proxy_plugin()
	{
		c.disconnect(); // optional, as it gets invoked on ~connection(), too.
	}

	void setup_proxy_enable(const x0::settings_value& cvar, const std::string& hostid)
	{
		proxy *px = acquire_proxy(hostid);
		cvar.load(px->enabled);
	}

	void setup_proxy_mode(const x0::settings_value& cvar, const std::string& hostid)
	{
		// TODO reverse / forward / transparent (forward)
	}

	void setup_proxy_origins(const x0::settings_value& cvar, const std::string& hostid)
	{
		proxy *px = acquire_proxy(hostid);
		cvar.load(px->origins);

		for (std::size_t i = 0, e = px->origins.size(); i != e; ++i)
		{
			std::string url = px->origins[i];
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
			server_.log(x0::severity::warn, "No origin servers defined for proxy at virtual-host: %s.", hostid.c_str());
	}

	void setup_proxy_hotspares(const x0::settings_value& cvar, const std::string& hostid)
	{
		//proxy *px = acquire_proxy(hostid);
		//cvar.load(px->hot_spares);
	}

	void setup_proxy_methods(const x0::settings_value& cvar, const std::string& hostid)
	{
		proxy *px = acquire_proxy(hostid);
		cvar.load(px->allowed_methods);
	}

	void setup_proxy_connect_timeout(const x0::settings_value& cvar, const std::string& hostid)
	{
		proxy *px = acquire_proxy(hostid);
		cvar.load(px->connect_timeout);
	}

	void setup_proxy_read_timeout(const x0::settings_value& cvar, const std::string& hostid)
	{
		proxy *px = acquire_proxy(hostid);
		cvar.load(px->read_timeout);
	}

	void setup_proxy_write_timeout(const x0::settings_value& cvar, const std::string& hostid)
	{
		proxy *px = acquire_proxy(hostid);
		cvar.load(px->write_timeout);
	}

	void setup_proxy_keepalive_timeout(const x0::settings_value& cvar, const std::string& hostid)
	{
		proxy *px = acquire_proxy(hostid);
		cvar.load(px->keepalive);
	}

	proxy *acquire_proxy(const std::string& hostid)
	{
		if (proxy *px = server_.context<proxy>(this, hostid))
			return px;

		proxy *px = server_.create_context<proxy>(this, hostid);
		px->loop = server_.loop();

		return px;
	}

	proxy *get_proxy(const std::string& hostid)
	{
		if (proxy *px = server_.context<proxy>(this, hostid))
			return px;

		return 0;
	}

	virtual void configure()
	{
		// XXX abuse as postconf hook

		// TODO ensure, that every proxy instance is properly equipped.
	}

private:
	void process(x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out)
	{
		proxy *px = get_proxy(in->hostid());
		if (!px)
			return next();

		if (!px->enabled)
			return next();

		if (!px->method_allowed(in->method))
		{
			out->status = x0::http_error::method_not_allowed;
			return next.done();
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
