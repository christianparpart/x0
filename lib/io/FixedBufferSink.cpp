/* <x0/FixedBufferSink.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/FixedBufferSink.h>
#include <x0/io/SinkVisitor.h>

namespace x0 {

void FixedBufferSink::accept(SinkVisitor& v)
{
    ;// TODO v.visit(*this);
}

ssize_t FixedBufferSink::write(const void *buffer, size_t size)
{
    buffer_.push_back(buffer, size);
    return size;
}

} // namespace x0
