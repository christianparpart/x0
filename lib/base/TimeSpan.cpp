/* <x0/TimeSpan.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/TimeSpan.h>
#include <x0/Buffer.h>
#include <string>

namespace x0 {

const TimeSpan TimeSpan::Zero(0lu);

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
