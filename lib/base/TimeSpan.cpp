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
    char tmp[64];
    int n = snprintf(tmp, sizeof(tmp), "%d.%02d:%02d:%02d.%03d",
        ts.days(), ts.hours(), ts.minutes(), ts.seconds(),
        ts.milliseconds());
    buf.push_back(tmp, n);
    return buf;
}
} // namespace x0
