/* <x0/NullSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/NullSource.h>

namespace x0 {

ssize_t NullSource::sendto(Sink& sink)
{
	return 0;
}

const char* NullSource::className() const
{
	return "NullSource";
}

} // namespace x0
