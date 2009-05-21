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
 * implements an x0-server
 */
class server :
	public noncopyable
{
private:
	typedef std::map<vhost_selector, vhost_ptr> vhost_map;

public:
	explicit server(asio::io_service& io_service);
	~server();

	// {{{ service control
	void configure();
	void start();
	void pause();
	void resume();
	void stop();
	// }}}

	// {{{ signals raised on request in order
	boost::signal<void(request&)> pre_processor;
	boost::signal<void(request&)> uri_mapper;
	boost::signal<void(request&)> uri_validator;
	boost::signal<void(request&)> document_root_resolver;
	handler<bool(request&, response&)> content_generator;
	boost::signal<void(request&, response&)> response_header_generator;
	boost::signal<void(request&, response&)> response_header_validator;
	boost::signal<void(request&, response&)> connection_logger;
	boost::signal<void(request&, response&)> post_processor;
	// }}}

	config& get_config();

	void setup_listener(int port, const std::string& bind_address = "0::0");
	void setup_plugin(plugin_ptr);

private:
	void handle_request(request& in, response& out);
	void generate_response_headers(request& in, response& out);
	listener_ptr listener_by_port(int port);

private:
	std::set<listener_ptr> listeners_;
	asio::io_service& io_service_;
	bool paused_;
	config config_;
	std::list<plugin_ptr> plugins_;
};

// {{{ module hooks
void vhost_init(server&);
void sendfile_init(server&);
void accesslog_init(server&);
// }}}

} // namespace x0

#endif
// vim:syntax=cpp
