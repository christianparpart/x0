#ifndef sw_x0_io_socket_sink_hpp
#define sw_x0_io_socket_sink_hpp 1

#include <x0/io/fd_sink.hpp>
#include <x0/io/buffer.hpp>
#include <x0/types.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** file descriptor stream sink.
 */
class X0_API socket_sink :
	public fd_sink
{
public:
	explicit socket_sink(tcp_socket& sock);

	tcp_socket& socket() const;

	template<typename CompletionHandler>
	void on_ready(CompletionHandler handler);

protected:
	tcp_socket& socket_;
};

//@}

// {{{ impl
inline socket_sink::socket_sink(tcp_socket& sock) :
	fd_sink(sock.native()),
	socket_(sock)
{
}

inline tcp_socket& socket_sink::socket() const
{
	return socket_;
}

template<typename CompletionHandler>
inline void socket_sink::on_ready(CompletionHandler handler)
{
	socket_.async_write_some(asio::null_buffers(), handler);
}
// }}}

} // namespace x0

#endif
