// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/io/CallbackSource.h>

namespace x0 {

CallbackSource::~CallbackSource()
{
    if (callback_) {
        callback_();
    }
}

ssize_t CallbackSource::size() const
{
    return 0;
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
