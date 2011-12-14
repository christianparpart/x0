/* <x0/io/CallbackSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2011 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/CallbackSource.h>

namespace x0 {

CallbackSource::~CallbackSource()
{
	if (callback_) {
		callback_(data_);
	}
}

ssize_t CallbackSource::sendto(Sink& sink)
{
	return 0;
}

const char* CallbackSource::className() const
{
	return "CallbackSource";
}

} // namespace x0
