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

public:
	async_writer(std::shared_ptr<socket_sink> snk, const source_ptr& src, const completion_handler_type& handler) :
		sink_(snk),
		source_(src),
		handler_(handler)
	{
	}

public:
	static void write(const std::shared_ptr<socket_sink>& snk, const source_ptr& src, const completion_handler_type& handler)
	{
		async_writer writer(snk, src, handler);

		int rv = snk->pump(*src);

		if (rv != -1 || errno != EAGAIN)
		{
			snk->on_ready(writer);
		}
	}

public:
	void operator()(const asio::error_code& ec, std::size_t bytes_transferred)
	{
		if (!ec)
		{
			int rv = sink_->pump(*source_);

			if (rv != -1 || errno == EAGAIN)
			{
				// call back as soon as sink is ready for more writes
				sink_->on_ready(*this);
			}
		}
		else
		{
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
