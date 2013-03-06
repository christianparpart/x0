/* <FileSink.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
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
	std::string path_;
	int flags_;
	int mode_;
	int handle_;

public:
	explicit FileSink(const std::string& filename, int flags = O_WRONLY | O_CREAT, int mode = 0666);
	~FileSink();

	int handle() const;

	virtual void accept(SinkVisitor& v);
	virtual ssize_t write(const void *buffer, size_t size);

	bool cycle();
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
