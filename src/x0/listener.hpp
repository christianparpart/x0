#ifndef x0_server_hpp
#define x0_server_hpp (1)

#include <x0/connection.hpp>
#include <x0/connection_manager.hpp>
#include <x0/request_handler.hpp>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <string>

namespace x0 {

class listener :
	public boost::noncopyable
{
public:
	explicit listener(boost::asio::io_service&);
	~listener();

	void configure(const std::string& address = "0::0", int port = 8080);
	void start();
	void stop();

	std::string address() const;
	int port() const;

private:
	/// handle completion of an async accept operation
	void handle_accept(const boost::system::error_code& e);

	/// handle a request to stop the listener.
	void handle_stop();

	boost::asio::io_service& io_service_;
	boost::asio::ip::tcp::acceptor acceptor_;
	connection_manager connection_manager_;
	request_handler request_handler_;
	connection_ptr new_connection_;
	std::string address_;
	int port_;
};

typedef boost::shared_ptr<listener> listener_ptr;

} // namespace x0

#endif
