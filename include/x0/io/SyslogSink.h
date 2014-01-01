/* <x0/io/SyslogSink.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/io/Sink.h>

namespace x0 {

class X0_API SyslogSink :
	public Sink
{
private:
	int level_;

public:
	explicit SyslogSink(int level);

	virtual void accept(SinkVisitor& v);
	virtual ssize_t write(const void *buffer, size_t size);

	static void open(const char* ident = nullptr, int options = 0, int facility = 0);
	static void close();
};

} // namespace x0
