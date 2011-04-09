/* <x0/BufferSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/BufferSource.h>

namespace x0 {

ssize_t BufferSource::sendto(Sink& sink)
{
	ssize_t result = sink.write(buffer_.data() + pos_, buffer_.size() - pos_);
	if (result > 0) {
		pos_ += result;
	}
	return result;
}

const char* BufferSource::className() const
{
	return "BufferSource";
}

} // namespace x0
