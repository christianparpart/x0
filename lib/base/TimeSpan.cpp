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
#include <cstdio>

namespace x0 {

const TimeSpan TimeSpan::Zero(static_cast<size_t>(0));

std::string TimeSpan::str() const
{
    Buffer b(64);
    b << *this;
    return b.str();
}

Buffer& operator<<(Buffer& buf, const TimeSpan& ts)
{
    bool green = false;

    // "5 days 5h 20m 33s"
    //        "5h 20m 33s"
    //           "20m 33s"
    //               "33s"

    if (ts.days()) {
        buf << ts.days() << "days";
        green = true;
    }

    if (green || ts.hours()) {
        if (green) buf << ' ';
        buf << ts.hours() << "h";
        green = true;
    }

    if (green || ts.minutes()) {
        if (green) buf << ' ';
        buf << ts.minutes() << "m";
        green = true;
    }

    if (green || ts.seconds()) {
        if (green) buf << ' ';
        buf << ts.seconds() << "s";
    }

    return buf;
}

} // namespace x0
