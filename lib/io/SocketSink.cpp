/* <x0/SocketSink.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/SocketSink.h>
#include <x0/io/SinkVisitor.h>
#include <x0/Socket.h>

namespace x0 {

SocketSink::SocketSink(Socket *s) :
	socket_(s)
{
}

void SocketSink::accept(SinkVisitor& v)
{
	v.visit(*this);
}

ssize_t SocketSink::write(const void *buffer, size_t size)
{
	return socket_->write(buffer, size);
}

} // namespace x0
