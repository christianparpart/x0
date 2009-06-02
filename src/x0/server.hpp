/* <x0/server.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_server_h
#define sw_x0_server_h

#include <x0/config.hpp>
#include <x0/logger.hpp>
#include <x0/listener.hpp>
#include <x0/handler.hpp>
#include <x0/plugin.hpp>
#include <x0/types.hpp>
#include <boost/signals.hpp>
#include <cstring>
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
	public boost::noncopyable
{
public:
	explicit server(boost::asio::io_service& io_service);
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
	/** is invoked once a new client connection is established */
	boost::signal<void(const connection_ptr&)> connection_open;

	/** is called at the very beginning of a request. */
	boost::signal<void(request&)> pre_process;

	/** resolves document_root to use for this request. */
	boost::signal<void(request&)> resolve_document_root;

	/** resolves request's physical filename (maps URI to physical path). */
	boost::signal<void(request&)> resolve_entity;

	/** generates response content for this request being processed. */
	handler generate_content;

	/** hook for generating accesslog logs and other things to be done after the request has been served. */
	boost::signal<void(request&, response&)> request_done;

	/** is called at the very end of a request. */
	boost::signal<void(request&, response&)> post_process;

	/** is called before a connection gets closed / or has been closed by remote point. */
	boost::signal<void(const connection_ptr&)> connection_close;
	// }}}

	/** 
	 * retrieves reference to server currently loaded configuration.
	 */
	config& get_config();

	/**
	 * writes a log entry into the server's error log.
	 */
	void log(const char *filename, unsigned int line, severity s, const char *msg);

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
	void drop_privileges(const std::string& user, const std::string& group);
	void handle_request(request& in, response& out);
	listener_ptr listener_by_port(int port);

private:
	std::list<listener_ptr> listeners_;
	boost::asio::io_service& io_service_;
	bool paused_;
	config config_;
	logger_ptr logger_;
	std::list<plugin_ptr> plugins_;
};

#define LOG(srv, severity, message...) (srv).log(__FILENAME__, __LINE__, severity, message)

} // namespace x0

#endif
// vim:syntax=cpp
