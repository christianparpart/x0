/* <x0/FilterSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FilterSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/BufferSink.h>
#include <x0/io/Filter.h>
#include <x0/io/SourceVisitor.h>
#include <x0/Defines.h>
#include <memory>

#if !defined(NDEBUG)
#	include <x0/StackTrace.h>
#endif

namespace x0 {

ssize_t FilterSource::sendto(Sink& sink)
{
	if (buffer_.empty()) {
		BufferSink input;
		source_->sendto(input);
		buffer_ = filter_(input.buffer());
		pos_ = 0;
	}

	ssize_t result = sink.write(buffer_.data() + pos_, buffer_.size() - pos_);

	if (result > 0) {
		pos_ += result;
	}

	return result;
}

BufferRef FilterSource::pull(Buffer& output)
{
	std::size_t pos = output.size();

	buffer_.clear();
	BufferRef input = source_->pull(buffer_);

	if (!input.empty() || force_)
	{
		Buffer filtered = filter_(input);
		output.push_back(filtered);

		//DEBUG("FilterSource: #%ld -> #%ld", input.size(), filtered.size());

		return output.ref(pos);
	} else
		return input;
}

void FilterSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

} // namespace x0
