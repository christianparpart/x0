/* <Cidr.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Cidr.h>
#include <x0/IPAddress.h>

namespace x0 {

std::string Cidr::str() const
{
	char result[INET6_ADDRSTRLEN + 32];

	inet_ntop(ipaddr_.family(), ipaddr_.data(), result, sizeof(result));

	size_t n = strlen(result);
	snprintf(result + n, sizeof(result) - n, "/%zu", prefix_);

	return result;
}

bool operator==(const Cidr& a, const Cidr& b)
{
	if (&a == &b)
		return true;

	if (a.address().family() != b.address().family())
		return false;

	switch (a.address().family()) {
		case AF_INET:
		case AF_INET6:
			return memcmp(a.address().data(), b.address().data(), a.address().size()) == 0 && a.prefix_ == b.prefix_;
		default:
			return false;
	}

	return false;
}

bool operator!=(const Cidr& a, const Cidr& b)
{
	return !(a == b);
}

} // namespace x0
