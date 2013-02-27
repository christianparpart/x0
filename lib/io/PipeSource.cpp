/* <x0/PipeSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/PipeSource.h>
#include <x0/io/BufferSink.h>
#include <x0/io/FileSink.h>
#include <x0/io/SocketSink.h>
#include <x0/io/PipeSink.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

PipeSource::PipeSource(Pipe* pipe) :
	pipe_(pipe)
{
}

PipeSource::~PipeSource()
{
}

ssize_t PipeSource::sendto(Sink& output)
{
	output.accept(*this);
	return result_;
}

void PipeSource::visit(BufferSink& sink)
{
	char buf[8 * 4096];
	result_ = pipe_->read(buf, sizeof(buf));

	if (result_ > 0) {
		sink.write(buf, result_);
	}
}

void PipeSource::visit(FileSink& sink)
{
	char buf[8 * 1024];
	result_ = pipe_->read(buf, sizeof(buf));

	if (result_> 0) {
		sink.write(buf, result_);
	}
}

void PipeSource::visit(SocketSink& sink)
{
	result_ = sink.write(pipe_, 8 * 1024);
}

void PipeSource::visit(PipeSink& sink)
{
	result_ = sink.write(pipe_, pipe_->size());
}

const char* PipeSource::className() const
{
	return "PipeSource";
}

} // namespace x0
