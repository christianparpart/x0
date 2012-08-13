/* <x0/FilterSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FilterSource.h>
#include <x0/io/BufferSink.h>
#include <x0/io/Filter.h>
#include <x0/Defines.h>
#include <memory>

namespace x0 {

FilterSource::~FilterSource()
{
	delete source_;
}

ssize_t FilterSource::sendto(Sink& sink)
{
	if (buffer_.empty()) {
		BufferSink input;

		ssize_t rv = source_->sendto(input);
		if (rv < 0 || (rv == 0 && !force_))
			return rv;

		buffer_ = (*filter_)(input.buffer());
	}

	ssize_t result = sink.write(buffer_.data() + pos_, buffer_.size() - pos_);

	if (result > 0) {
		pos_ += result;
		if (pos_ == buffer_.size()) {
			pos_ = 0;
			buffer_.clear();
		}
	}

	return result;
}

const char* FilterSource::className() const
{
	return "FilterSource";
}

} // namespace x0
