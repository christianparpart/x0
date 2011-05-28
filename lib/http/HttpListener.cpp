/* <x0/HttpListener.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpListener.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpServer.h>
#include <x0/SocketDriver.h>
#include <x0/SocketSpec.h>
#include <x0/sysconfig.h>
#include <fcntl.h>

namespace x0 {

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

HttpListener::HttpListener(HttpServer& srv) : 
#ifndef NDEBUG
	Logging("HttpListener"),
#endif
	socket_(srv.loop()),
	server_(srv),
	errorCount_(0)
{
	socket_.set<HttpListener, &HttpListener::callback>(this);
}

HttpListener::~HttpListener()
{
	TRACE("~HttpListener(): %s:%d", socket_.address().c_str(), socket_.port());
	stop();
}

int HttpListener::backlog() const
{
	return socket_.backlog();
}

void HttpListener::setBacklog(int value)
{
	socket_.setBacklog(value);
}

bool HttpListener::open(const SocketSpec& spec)
{
	if (socket_.open(spec, O_CLOEXEC | O_NONBLOCK))
		return true;

	log(Severity::error, "Error listening on socket (%s): %s", spec.str().c_str(), socket_.errorText().c_str());
	return false;
}

bool HttpListener::open(const std::string& unixPath)
{
	if (socket_.open(unixPath, O_CLOEXEC | O_NONBLOCK))
		return true;

	log(Severity::error, "Error listening on UNIX socket (%s): %s", unixPath.c_str(), socket_.errorText().c_str());
	return false;
}

bool HttpListener::open(const std::string& address, int port)
{
	if (socket_.open(address, port, O_CLOEXEC | O_NONBLOCK))
		return true;

	log(Severity::error, "Error listening on TCP/IP address %s, port %d: %s", address.c_str(), port, socket_.errorText().c_str());
	return false;
}

void HttpListener::start()
{
	TRACE("starting");
	socket_.start();
}

void HttpListener::stop()
{
	TRACE("stopping");
	socket_.stop();
}

void HttpListener::callback(Socket* cs, ServerSocket*)
{
	server_.selectWorker()->enqueue(std::make_pair(cs, this));
}

} // namespace x0
