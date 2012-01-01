/* <x0/BufferRefSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/BufferRefSource.h>

namespace x0 {

ssize_t BufferRefSource::sendto(Sink& sink)
{
	if (pos_ == buffer_.size())
		return 0;

	ssize_t result = sink.write(buffer_.data() + pos_, buffer_.size() - pos_);

	if (result > 0)
		pos_ += result;

	return result;
}

const char* BufferRefSource::className() const
{
	return "BufferRefSource";
}

} // namespace x0
