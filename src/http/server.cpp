#include <http/server.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

namespace http {

server::server(boost::asio::io_service& io_service)
  : io_service_(io_service),
	acceptor_(io_service),
	paused_(false),
	connection_manager_(),
	request_handler_("."),
	new_connection_(new connection(io_service_, connection_manager_, request_handler_)),
	address_(),
	port_(-1)
{
}

server::~server()
{
	stop();
}

template<typename T>
static std::string itoa(T&& value)
{
	std::stringstream sstr;
	sstr << value;
	return sstr.str();
}

void server::start(const std::string& address, int port)
{
	boost::asio::ip::tcp::resolver resolver(io_service_);
	boost::asio::ip::tcp::resolver::query query(address, itoa(8080));
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

	paused_ = false;

	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::linger(false, 0));
	acceptor_.set_option(boost::asio::ip::tcp::no_delay(true));
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::keep_alive(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();

	std::cout << "x0d server listening on: " << endpoint << std::endl;

	acceptor_.async_accept(new_connection_->socket(),
		boost::bind(&server::handle_accept, this, boost::asio::placeholders::error));

	address_ = address;
	port_ = port;
}

void server::handle_accept(const boost::system::error_code& e)
{
	if (e)
		return;

	connection_manager_.start(new_connection_);
	new_connection_.reset(new connection(io_service_, connection_manager_, request_handler_));

	acceptor_.async_accept(new_connection_->socket(),
		boost::bind(&server::handle_accept, this, boost::asio::placeholders::error));
}

void server::stop()
{
	// the server is stopped by cancelling  all outstanding (async) operations.
	// Once all operations have finished the io_servie_.run() call will exit.
	io_service_.post(boost::bind(&server::handle_stop, this));
}

void server::handle_stop()
{
	acceptor_.close();
	connection_manager_.stop_all();
}

void server::pause()
{
	paused_ = true;
}

bool server::paused() const
{
	return paused_;
}

void server::resume()
{
	paused_ = false;
}

std::string server::address() const
{
	return address_;
}

int server::port() const
{
	return port_;
}

} // namespace http
