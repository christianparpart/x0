/* <x0/BufferSink.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/BufferSink.h>
#include <x0/io/SinkVisitor.h>

namespace x0 {

void BufferSink::accept(SinkVisitor& v)
{
    v.visit(*this);
}

ssize_t BufferSink::write(const void *buffer, size_t size)
{
    buffer_.push_back(buffer, size);
    return size;
}

} // namespace x0
