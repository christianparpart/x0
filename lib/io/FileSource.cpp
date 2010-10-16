/* <x0/FileSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FileSource.h>
#include <x0/io/SourceVisitor.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

/** initializes a file source.
 *
 * \param f the file to stream
 */
FileSource::FileSource(int fd, std::size_t offset, std::size_t size, bool autoClose) :
	SystemSource(fd, offset, size),
	autoClose_(autoClose)
{
}

FileSource::~FileSource()
{
	if (autoClose_)
		::close(handle());
}

void FileSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

} // namespace x0
