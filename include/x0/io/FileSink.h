/* <FileSink.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_io_FileSink_hpp
#define sw_x0_io_FileSink_hpp 1

#include <x0/io/Sink.h>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

//! \addtogroup io
//@{

/** file sink.
 */
class X0_API FileSink :
	public Sink
{
private:
	int handle_;

public:
	explicit FileSink(const std::string& filename, int flags = O_WRONLY | O_CREAT);
	~FileSink();

	int handle() const;

	virtual void accept(SinkVisitor& v);
	virtual ssize_t write(const void *buffer, size_t size);
};

//@}

// {{{ inlines
inline int FileSink::handle() const
{
	return handle_;
}
// }}}

} // namespace x0

#endif
