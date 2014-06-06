/* <x0/io/CompositeSource.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/CompositeSource.h>
#include <deque>

namespace x0 {

CompositeSource::~CompositeSource()
{
    clear();
}

ssize_t CompositeSource::sendto(Sink& sink)
{
    ssize_t result = 0;

    while (!empty()) {
        Source* front = sources_.front().get();
        ssize_t rv = front->sendto(sink);

        if (rv < 0) { // error in source
            return result ? result : rv;
        } else if (rv == 0) { // empty source
            sources_.pop_front();
        } else {
            result += rv;
        }
    }

    return result;
}

const char* CompositeSource::className() const
{
    return "CompositeSource";
}

} // namespace x0
