/* <x0/EmptySource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/EmptySource.h>

namespace x0 {

ssize_t EmptySource::sendto(Sink& sink)
{
	return 0;
}

const char* EmptySource::className() const
{
	return "EmptySource";
}

} // namespace x0
