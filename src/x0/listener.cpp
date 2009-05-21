#include <x0/listener.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

namespace x0 {

listener::listener(io_service& io_service, const request_handler_fn& handler)
  : io_service_(io_service),
	acceptor_(io_service),
	connection_manager_(),
	handler_(handler),
	new_connection_(new connection(io_service_, connection_manager_, handler_)),
	address_(),
	port_(-1)
{
}

listener::~listener()
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

void listener::configure(const std::string& address, int port)
{
	address_ = address;
	port_ = port;
}

void listener::start()
{
	ip::tcp::resolver resolver(io_service_);
	ip::tcp::resolver::query query(address_, itoa(port_));
	ip::tcp::endpoint endpoint = *resolver.resolve(query);

	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(ip::tcp::acceptor::reuse_address(true));
	acceptor_.set_option(ip::tcp::acceptor::linger(false, 0));
	acceptor_.set_option(ip::tcp::no_delay(true));
	acceptor_.set_option(ip::tcp::acceptor::keep_alive(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();

	acceptor_.async_accept(new_connection_->socket(),
		bind(&listener::handle_accept, this, placeholders::error));
}

void listener::handle_accept(const system::error_code& e)
{
	if (e)
		return;

	connection_manager_.start(new_connection_);
	new_connection_.reset(new connection(io_service_, connection_manager_, handler_));

	acceptor_.async_accept(new_connection_->socket(),
		bind(&listener::handle_accept, this, placeholders::error));
}

void listener::stop()
{
	// the listener is stopped by cancelling  all outstanding (async) operations.
	// Once all operations have finished the io_servie_.run() call will exit.
	io_service_.post(bind(&listener::handle_stop, this));
}

void listener::handle_stop()
{
	acceptor_.close();
	connection_manager_.stop_all();
}


std::string listener::address() const
{
	return address_;
}

int listener::port() const
{
	return port_;
}

} // namespace x0
