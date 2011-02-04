/* <x0/io/CompositeSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/CompositeSource.h>
#include <deque>

namespace x0 {

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
