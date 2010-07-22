/* <SystemSink.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_SystemSink_hpp
#define sw_x0_io_SystemSink_hpp 1

#include <x0/io/Sink.h>
#include <x0/Buffer.h>

namespace x0 {

//! \addtogroup io
//@{

/** file descriptor stream sink.
 */
class X0_API SystemSink :
	public Sink
{
public:
	explicit SystemSink(int fd);

	void async(bool value);
	bool async() const;

	int handle() const;

	virtual ssize_t pump(Source& src);

protected:
	Buffer buf_;
	off_t offset_;
	int handle_;
};

//@}

inline int SystemSink::handle() const
{
	return handle_;
}

} // namespace x0

#endif
