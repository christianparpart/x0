#ifndef sw_x0_async_pump_hpp
#define sw_x0_async_pump_hpp 1

#include <x0/io/source.hpp>
#include <x0/io/socket_sink.hpp>
#include <x0/types.hpp>
#include <asio.hpp>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

void async_write(tcp_socket& target, const source_ptr& source, const completion_handler_type& handler);
void async_write(const std::shared_ptr<socket_sink>& snk, const source_ptr& source, const completion_handler_type& handler);

//@}

// {{{
class async_writer
{
private:
	std::shared_ptr<socket_sink> sink_;
	source_ptr source_;
	completion_handler_type handler_;
	std::size_t bytes_transferred_;

public:
	async_writer(std::shared_ptr<socket_sink> snk, const source_ptr& src, const completion_handler_type& handler) :
		sink_(snk),
		source_(src),
		handler_(handler),
		bytes_transferred_(0)
	{
	}

public:
	static void write(const std::shared_ptr<socket_sink>& snk, const source_ptr& src, const completion_handler_type& handler)
	{
		async_writer writer(snk, src, handler);

		writer.write();
	}

public:
	void operator()(const asio::error_code& ec, std::size_t bytes_transferred)
	{
		if (!ec)
		{
			write();
		}
		else
		{
			handler_(ec, bytes_transferred);
		}
	}

private:
	void write()
	{
		ssize_t rv = sink_->pump(*source_);

		if (rv == 0)
		{
			// finished
			handler_(asio::error_code(), bytes_transferred_);
		}
		else if (rv > 0)
		{
			// we wrote something (if not even all) -> call back when fully transmitted
			bytes_transferred_ += rv;
			sink_->on_ready(*this);
		}
		else if (errno == EAGAIN || errno == EINTR)
		{
			// call back as soon as sink is ready for more writes
			sink_->on_ready(*this);
		}
	}
};

inline void async_write(tcp_socket& target, const source_ptr& src, const completion_handler_type& handler)
{
	async_write(std::make_shared<socket_sink>(target), src, handler);
}

inline void async_write(const std::shared_ptr<socket_sink>& snk, const source_ptr& src, const completion_handler_type& handler)
{
	async_writer::write(snk, src, handler);
}
// }}}

} // namespace x0

#endif
