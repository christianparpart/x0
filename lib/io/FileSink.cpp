/* <x0/FileSink.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/FileSink.h>
#include <x0/io/SinkVisitor.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

/*! Initializes a file sink.
 *
 * \param filename path to the fielsystem's file.  The filename \b /dev/stdout and \b /dev/stderr
 *                 are handled specially, not directly opening that file, but assigning
 *                 its well known file descriptors (1 and 2) directly.
 *                 These file descriptors, unlike any other, are not closed on destruction.
 * \param flags    Flags, passed to the open(2) system call, e.g. O_WRONLY.
 *
 * If possible, the FD_CLOEXEC flag is enabled after a successful open(2), too, but
 * if you rely on this behavior and are in a multi threaded environment with
 * fork()'s in between, it is generally recommended to pass \b O_CLOEXEC as \a flag aswell.
 */

FileSink::FileSink(const std::string& filename, int flags)
{
	if (filename == "/dev/stdout")
		handle_ = 1;
	else if (filename == "/dev/stderr")
		handle_ = 2;
	else {
		handle_ = open(filename.c_str(), flags, 0666);
		if (handle_ >= 0) {
			fcntl(handle_, F_SETFD, fcntl(handle_, F_GETFD) | FD_CLOEXEC);
		}
	}
}

/*! Frees object's file-descriptor.
 *
 * \note As mentioned in the constructor, \b /dev/stdout and \b /dev/stderr are not closed.
 */
FileSink::~FileSink()
{
	if (handle_ >= 3) {
		::close(handle_);
	}
}

void FileSink::accept(SinkVisitor& v)
{
	v.visit(*this);
}

ssize_t FileSink::write(const void *buffer, size_t size)
{
	return ::write(handle_, buffer, size);
}

} // namespace x0
