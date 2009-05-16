#ifndef x0_http_connection_hpp
#define x0_http_connection_hpp (1)

#include <http/connection.hpp>
#include <http/reply.hpp>
#include <http/request.hpp>
#include <http/request_handler.hpp>
#include <http/request_parser.hpp>

#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <string>

namespace http {

class connection_manager;

class connection :
	public boost::enable_shared_from_this<connection>,
	private boost::noncopyable {
public:
	connection(boost::asio::io_service& io_service,
		connection_manager& manager, request_handler& handler);

	~connection();

	/// get the connection socket handle
	boost::asio::ip::tcp::socket& socket();

	/// start first async operation for this connection
	void start();

	/// stop all async operations associated with this connection
	void stop();

private:
	void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);
	void handle_write(const boost::system::error_code& e);

	boost::asio::ip::tcp::socket socket_;
	connection_manager& connection_manager_;
	request_handler& request_handler_;

	/// buffer for incoming data.
	boost::array<char, 8192> buffer_;

	/// http request
	request request_;

	request_parser request_parser_;

	reply reply_;
};

typedef boost::shared_ptr<connection> connection_ptr;

} // namespace http

#endif
