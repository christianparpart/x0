/* <plugins/director/HttpHealthMonitor.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include "HealthMonitor.h"
#include <x0/Buffer.h>
#include <x0/Socket.h>
#include <ev++.h>


/**
 * HTTP Health Monitor.
 */
class HttpHealthMonitor :
	public HealthMonitor
{
public:
	explicit HttpHealthMonitor(x0::HttpWorker& worker);
	~HttpHealthMonitor();

	virtual void setRequest(const char* fmt, ...);
	virtual void reset();
	virtual void onCheckStart();

private:
	void onConnectDone(x0::Socket*, int revents);
	void io(x0::Socket*, int revents);
	void writeSome();
	void readSome();
	void onTimeout(x0::Socket* s);

private:
	x0::Socket socket_;
	x0::Buffer request_;
	size_t writeOffset_;
	x0::Buffer response_;
};
