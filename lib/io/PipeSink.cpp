/* <x0/PipeSink.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/Pipe.h>
#include <x0/io/PipeSink.h>
#include <x0/io/SinkVisitor.h>

namespace x0 {

PipeSink::PipeSink(Pipe* pipe) :
	pipe_(pipe)
{
}

PipeSink::~PipeSink()
{
}

void PipeSink::accept(SinkVisitor& v)
{
	v.visit(*this);
}

ssize_t PipeSink::write(const void *buffer, size_t size)
{
	return pipe_->write(buffer, size);
}

} // namespace x0
