/* <x0/connection.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/connection.hpp>
#include <x0/connection_manager.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/debug.hpp>
#include <x0/types.hpp>
#include <boost/bind.hpp>

namespace x0 {

connection::connection(
	boost::asio::io_service& io_service, 
	connection_manager& manager, const request_handler_fn& handler)
  : secure(false),
	socket_(io_service),
	connection_manager_(manager),
	request_handler_(handler),
	buffer_(),
	request_(new request(*this)),
	request_parser_(),
	strand_(io_service)
{
	DEBUG("connection(%p)", this);
}

connection::~connection()
{
	DEBUG("~connection(%p)", this);
}

void connection::start()
{
	DEBUG("connection(%p).start()", this);

	socket_.async_read_some(boost::asio::buffer(buffer_),
		bind(&connection::handle_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void connection::resume()
{
	DEBUG("connection(%p).resume()", this);

	request_parser_.reset();

	socket_.async_read_some(boost::asio::buffer(buffer_),
		bind(&connection::handle_read, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void connection::stop()
{
	DEBUG("connection(%p).stop()", this);
	socket_.close();
}

/**
 * This method gets invoked when there is data in our connection ready to read.
 *
 * We assume, that we are in request-parsing state.
 */
void connection::handle_read(const boost::system::error_code& e, std::size_t bytes_transferred)
{
	DEBUG("connection(%p).handle_read(sz=%ld)", this, bytes_transferred);

	if (!e)
	{
		// parse request (partial)
		boost::tribool result;
		try
		{
			boost::tie(result, boost::tuples::ignore) = request_parser_.parse(
				*request_, buffer_.data(), buffer_.data() + bytes_transferred);
		}
		catch (response::code_type code)
		{
			// we encountered a request parsing error, bail out
			fprintf(stderr, "response::code exception caught (%d %s)\n", code, response::status_cstr(code));;
			fflush(stderr);

			(new response(shared_from_this(), request_, code))->flush();
		}

		if (result) // request fully parsed
		{
			if (response *response_ = new response(shared_from_this(), request_))
			{
				try
				{
					request_handler_(*request_, *response_);
				}
				catch (response::code_type reply)
				{
					fprintf(stderr, "response::code exception caught (%d %s)\n", reply, response::status_cstr(reply));
					fflush(stderr);
					response_->status = reply;
					response_->flush();
				}
			}
			else
			{
				// ran out of memory?
				connection_manager_.stop(shared_from_this());
			}
		}
		else if (!result) // received an invalid request
		{
			// -> send stock response: BAD_REQUEST
			(new response(shared_from_this(), request_, response::bad_request))->flush();
		}
		else // request still incomplete
		{
			// -> continue reading for request
			socket_.async_read_some(boost::asio::buffer(buffer_),
				bind(&connection::handle_read, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
		}
	}
	else if (e != boost::asio::error::operation_aborted)
	{
		// some connection error (other than operation_aborted) happened
		// -> kill this connection.
		connection_manager_.stop(shared_from_this());
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
void connection::response_transmitted(const boost::system::error_code& e)
{
	DEBUG("connection(%p).response_transmitted()", this);

	if (!e)
	{
		boost::system::error_code ignored;
		socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
	}

	if (e != boost::asio::error::operation_aborted)
	{
		connection_manager_.stop(shared_from_this());
	}
}

} // namespace x0
