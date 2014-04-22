/* <x0/io/CallbackSource.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/CallbackSource.h>

namespace x0 {

CallbackSource::~CallbackSource()
{
    if (callback_) {
        callback_();
    }
}

ssize_t CallbackSource::sendto(Sink& sink)
{
    return 0;
}

const char* CallbackSource::className() const
{
    return "CallbackSource";
}

} // namespace x0
