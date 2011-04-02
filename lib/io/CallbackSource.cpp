/* <x0/io/CallbackSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2011 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/CallbackSource.h>

namespace x0 {

ssize_t CallbackSource::sendto(Sink& sink)
{
	if (!callback_) {
		errno = EINVAL;
		return -1;
	}

	callback_();
	return 0;
}

} // namespace x0
