/* <x0/TimeSpan.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/TimeSpan.h>
#include <x0/Buffer.h>
#include <string>
#include <cstdint>

namespace x0 {

const TimeSpan TimeSpan::Zero(static_cast<size_t>(0));

std::string TimeSpan::str() const
{
	int totalMinutes =
		days() * 24 * 60 +
		hours() * 60 +
		minutes();

	Buffer b(64);
	b << totalMinutes << "m " << seconds() << "s";
	return b.str();
}

} // namespace x0
