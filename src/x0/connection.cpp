/* <x0/connection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/connection.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/server.hpp>
#include <x0/types.hpp>
#include <boost/bind.hpp>

namespace x0 {

connection::connection(x0::server& srv)
  : secure(false),
	server_(srv),
	socket_(server_.io_service()),
	timer_(server_.io_service()),
	client_ip_(),
	client_port_(0),
	buffer_(8192),
	request_(new request(*this)),
	request_reader_()
{
	//DEBUG("connection(%p)", this);
}

connection::~connection()
{
	//DEBUG("~connection(%p)", this);

	try
	{
		server_.connection_close(this); // we cannot pass a shared pointer here as use_count is already zero and it would just lead into an exception though
		socket_.close();
	}
	catch (...)
	{
		DEBUG("~connection(%p): unexpected exception", this);
	}

	delete request_;
}

void connection::start()
{
	//DEBUG("connection(%p).start()", this);

	server_.connection_open(boost::bind(&connection::async_read_some, this), shared_from_this());
}

void connection::resume()
{
	//DEBUG("connection(%p).resume()", this);

	request_reader_.reset();
	request_ = new request(*this);

	async_read_some();
}

void connection::async_read_some()
{
	if (server_.max_read_idle() != -1)
	{
		timer_.expires_from_now(boost::posix_time::seconds(server_.max_read_idle()));
		timer_.async_wait(boost::bind(&connection::read_timeout, this, asio::placeholders::error));
	}

	socket_.async_read_some(
		buffer_.asio_avail(), //(asio::buffer(buffer_),
		bind(
			&connection::handle_read, shared_from_this(),
			asio::placeholders::error,
			asio::placeholders::bytes_transferred
		)
	);
}

void connection::read_timeout(const asio::error_code& ec)
{
	if (ec != asio::error::operation_aborted)
	{
		//DEBUG("connection(%p): read timed out", this);
		socket_.cancel();
	}
}

/**
 * This method gets invoked when there is data in our connection ready to read.
 *
 * We assume, that we are in request-parsing state.
 */
void connection::handle_read(const asio::error_code& e, std::size_t bytes_transferred)
{
	//DEBUG("connection(%p).handle_read(ec=%s, sz=%ld)", this, e.message().c_str(), bytes_transferred);
	timer_.cancel();

	if (!e)
	{
		// parse request (partial)
		boost::tribool result = request_reader_.parse(*request_, buffer_.data(), buffer_.data() + bytes_transferred);

		if (result) // request fully parsed
		{
			if (response *response_ = new response(shared_from_this(), request_))
			{
				request_ = 0;

				try
				{
					server_.handle_request(response_->request(), *response_);
				}
				catch (response::code_type reply)
				{
//					fprintf(stderr, "response::code exception caught (%d %s)\n", reply, response::status_cstr(reply));
//					fflush(stderr);
					response_->status = reply;
					response_->flush();
				}
			}
		}
		else if (!result) // received an invalid request
		{
			// -> send stock response: BAD_REQUEST
			(new response(shared_from_this(), request_, response::bad_request))->flush();
			request_ = 0;
		}
		else // result indeterminate: request still incomplete
		{
			// -> continue reading for request
			async_read_some();
		}
	}
}

void connection::write_timeout(const asio::error_code& ec)
{
	if (ec != asio::error::operation_aborted)
	{
		//DEBUG("connection(%p): write timed out", this);
		socket_.cancel();
	}
}

/**
 * this method gets invoked when response write operation has finished.
 *
 * \param response the response object just transmitted
 * \param e transmission error code
 *
 * We will fully shutown the TCP connection.
 */
void connection::response_transmitted(const asio::error_code& ec)
{
	//DEBUG("connection(%p).response_transmitted(%s)", this, ec.message().c_str());

	if (!ec)
	{
		asio::error_code ignored;
		socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
	}
}

std::string connection::client_ip() const
{
	if (client_ip_.empty())
	{
		client_ip_ = socket_.remote_endpoint().address().to_string();
	}

	return client_ip_;
}

int connection::client_port() const
{
	if (!client_port_)
	{
		client_port_ = socket_.remote_endpoint().port();
	}

	return client_port_;
}

} // namespace x0
