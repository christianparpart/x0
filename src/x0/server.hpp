/* <x0/server.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_server_h
#define sw_x0_server_h

#include <x0/listener.hpp>
#include <x0/handler.hpp>
#include <x0/vhost.hpp>
#include <x0/vhost_selector.hpp>
#include <x0/plugin.hpp>
#include <x0/types.hpp>
#include <boost/signals.hpp>
#include <set>

namespace x0 {

/**
 * \ingroup core
 * \brief implements the x0 web server.
 *
 * \see connection, request, response, plugin
 * \see server::start(), server::stop()
 */
class server :
	public noncopyable
{
public:
	explicit server(asio::io_service& io_service);
	~server();

	// {{{ service control
	/** configures this server as defined in the configuration section(s). */
	void configure();
	/** starts this server object by listening on new connections and processing them. */
	void start();
	/** pauses an already active server by not accepting further new connections until resumed. */
	void pause();
	/** resumes a currently paused server by continueing processing new connections. */
	void resume();
	/** gracefully stops a running server */
	void stop();
	// }}}

	// {{{ signals raised on request in order
	/** is called at the very beginning of a request. */
	boost::signal<void(request&)> pre_processor;

	/** resolves document_root to use for this request. */
	boost::signal<void(request&)> document_root_resolver;

	/** maps a request URI to a physical path. */
	boost::signal<void(request&)> uri_mapper;

	/** generates response content for this request being processed. */
	handler<bool(request&, response&)> content_generator;

	/** generates response headers for this request being processed. */
	boost::signal<void(request&, response&)> response_header_generator;

	/** hook for generating access logs and other things to be logged per request/response. */
	boost::signal<void(request&, response&)> access_logger;

	/** is called at the very end of a request. */
	boost::signal<void(request&, response&)> post_processor;
	// }}}

	/** 
	 * retrieves reference to server currently loaded configuration.
	 */
	config& get_config();

	/**
	 * sets up a TCP/IP listener on given bind_address and port.
	 *
	 * If there is already a listener on this bind_address:port pair
	 * then no error will be raised.
	 */
	void setup_listener(int port, const std::string& bind_address = "0::0");

	/**
	 * loads a new plugin into the server.
	 *
	 * \see plugin
	 */
	void setup_plugin(plugin_ptr plug);

private:
	void handle_request(request& in, response& out);
	listener_ptr listener_by_port(int port);

private:
	std::set<listener_ptr> listeners_;
	asio::io_service& io_service_;
	bool paused_;
	config config_;
	std::list<plugin_ptr> plugins_;
};

} // namespace x0

// {{{ module hooks
extern "C" void vhost_init(x0::server&);
extern "C" void mime_init(x0::server&);
extern "C" void sendfile_init(x0::server&);
extern "C" void accesslog_init(x0::server&);
// }}}

#endif
// vim:syntax=cpp
