/* <x0/local_stream.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_local_stream_hpp
#define sw_x0_local_stream_hpp 1

#include <x0/api.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace x0 {

//! \addtogroup base
//@{

/** provides a socket pair (local stream) API.
 */
class local_stream
{
	local_stream& operator=(const local_stream& v) = delete;
	local_stream(const local_stream&) = delete;

public:
	local_stream();
	~local_stream();

	int local();
	int remote();

	void close();

private:
	int pfd_[2];
};

// {{{ inlines
inline local_stream::local_stream()
{
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pfd_) < 0)
	{
		pfd_[0] = pfd_[1] = -1;
	}
}

inline local_stream::~local_stream()
{
	close();
}

inline int local_stream::local()
{
	return pfd_[0];
}

inline int local_stream::remote()
{
	return pfd_[1];
}

inline void local_stream::close()
{
	if (pfd_[0] != -1)
		::close(pfd_[0]);

	if (pfd_[1] != -1)
		::close(pfd_[1]);
}
// }}}

//@}

} // namespace x0

#endif
