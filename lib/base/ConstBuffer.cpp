/* <src/ConstBuffer.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/ConstBuffer.h>

namespace x0 {

bool ConstBuffer::setCapacity(std::size_t /*n*/)
{
	return false;
}

}
