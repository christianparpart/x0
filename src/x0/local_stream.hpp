/* <x0/local_stream.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_local_stream_hpp
#define sw_x0_local_stream_hpp 1

#include <x0/api.hpp>
#include <asio.hpp>
#include <boost/noncopyable.hpp>

namespace x0 {

//! \addtogroup base
//@{

/** provides a socket pair (local stream) API.
 */
class local_stream :
	public boost::noncopyable
{
public:
	typedef asio::local::stream_protocol::socket socket;

public:
	explicit local_stream(asio::io_service& io);
	~local_stream();

	socket& local();
	socket& remote();

	void close();

private:
	asio::io_service& io_;
	socket local_;
	socket remote_;
};

// {{{ inlines
inline local_stream::local_stream(asio::io_service& io) :
	io_(io), local_(io), remote_(io)
{
	asio::local::connect_pair(local_, remote_);
}

inline local_stream::~local_stream()
{
	close();
}

inline local_stream::socket& local_stream::local()
{
	return local_;
}

inline local_stream::socket& local_stream::remote()
{
	return remote_;
}

inline void local_stream::close()
{
	local_.close();
	remote_.close();
}
// }}}

//@}

} // namespace x0

#endif
