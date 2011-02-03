/* <x0/io/CompositeSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/CompositeSource.h>
#include <x0/io/SourceVisitor.h>
#include <x0/BufferRef.h>
#include <x0/Buffer.h>

#include <deque>

namespace x0 {

BufferRef CompositeSource::pull(Buffer& output)
{
	while (!empty())
	{
		if (BufferRef ref = sources_.front()->pull(output))
			return ref;

		sources_.pop_front();
	}

	return BufferRef();
}

void CompositeSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

ssize_t CompositeSource::sendto(Sink& sink)
{
	ssize_t result = 0;

	while (!empty()) {
		result = sources_.front()->sendto(sink);
		if (result < 0)
			return result;

		sources_.pop_front();
	}

	return result;
}

} // namespace x0
