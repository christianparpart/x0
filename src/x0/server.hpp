#ifndef sw_x0_server_h
#define sw_x0_server_h

#include <http/server.hpp>
#include <x0/vhost.hpp>
#include <x0/vhost_selector.hpp>
#include <set>

namespace x0 {

/**
 * implements an x0-server
 */
class server :
	public boost::noncopyable
{
private:
	typedef std::map<vhost_selector, vhost_ptr> vhost_map;

public:
	explicit server(boost::asio::io_service& io_service);
	~server();

	void configure();

	void start();
	void pause();
	void resume();
	void stop();

private:
	http::server_ptr listener_by_port(int port);

private:
	std::set<http::server_ptr> listeners_;
	boost::asio::io_service& io_service_;
	vhost_map vhosts_;
};

} // namespace x0

#endif
// vim:syntax=cpp
