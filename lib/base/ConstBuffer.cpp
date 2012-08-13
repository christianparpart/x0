/* <src/ConstBuffer.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/ConstBuffer.h>

namespace x0 {

bool ConstBuffer::setCapacity(std::size_t /*n*/)
{
	return false;
}

}
