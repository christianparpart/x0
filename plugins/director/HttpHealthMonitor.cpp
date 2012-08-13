/* <plugins/director/HttpHealthMonitor.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "HttpHealthMonitor.h"
#include "Director.h"
#include "Backend.h"
#include <cassert>
#include <cstdarg>

/*
 * TODO
 * - connect/read/write timeout handling.
 */

using namespace x0;

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->debug(msg))
#else
#	define TRACE(msg...) do { } while (0)
#endif

HttpHealthMonitor::HttpHealthMonitor(HttpWorker& worker) :
	HealthMonitor(worker),
	socket_(worker_.loop()),
	request_(),
	writeOffset_(0),
	response_()
{
}

HttpHealthMonitor::~HttpHealthMonitor()
{
}

void HttpHealthMonitor::reset()
{
	HealthMonitor::reset();

	socket_.close();

	writeOffset_ = 0;
	response_.clear();
}

/**
 * Sets the raw HTTP request, used to perform the health check.
 */
void HttpHealthMonitor::setRequest(const char* fmt, ...)
{
	va_list va;
	size_t blen = std::min(request_.capacity(), 1023lu);

	do {
		request_.reserve(blen + 1);
		va_start(va, fmt);
		blen = vsnprintf(const_cast<char*>(request_.data()), request_.capacity(), fmt, va);
		va_end(va);
	} while (blen >= request_.capacity());

	request_.resize(blen);
}

/**
 * Callback, timely invoked when a health check is to be started.
 */
void HttpHealthMonitor::onCheckStart()
{
	TRACE("onCheckStart()");

	socket_.open(backend_->socketSpec(), O_NONBLOCK | O_CLOEXEC);

	if (!socket_.isOpen()) {
		TRACE("Connect failed. %s", strerror(errno));
		logFailure();
	} else if (socket_.state() == Socket::Connecting) {
		TRACE("connecting asynchronously.");
		socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(this, backend_->director()->connectTimeout());
		socket_.setReadyCallback<HttpHealthMonitor, &HttpHealthMonitor::onConnectDone>(this);
		socket_.setMode(Socket::ReadWrite);
	} else {
		socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(this, backend_->director()->writeTimeout());
		socket_.setReadyCallback<HttpHealthMonitor, &HttpHealthMonitor::io>(this);
		socket_.setMode(Socket::ReadWrite);
		TRACE("connected.");
	}
}

/**
 * Callback, invoked on completed asynchronous connect-operation.
 */
void HttpHealthMonitor::onConnectDone(Socket*, int revents)
{
	TRACE("onConnectDone(0x%04x)", revents);

	if (socket_.state() == Socket::Operational) {
		TRACE("connected");
		socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(this, backend_->director()->writeTimeout());
		socket_.setReadyCallback<HttpHealthMonitor, &HttpHealthMonitor::io>(this);
		socket_.setMode(Socket::ReadWrite);
	} else {
		TRACE("Asynchronous connect failed %s", strerror(errno));
		logFailure();
	}
}

/**
 * Callback, invoked on I/O readiness of origin server connection.
 */
void HttpHealthMonitor::io(Socket*, int revents)
{
	TRACE("io(0x%04x)", revents);

	if (revents & ev::WRITE) {
		writeSome();
	}

	if (revents & ev::READ) {
		readSome();
	}
}

/**
 * Writes the request chunk to the origin server.
 */
void HttpHealthMonitor::writeSome()
{
	TRACE("writeSome()");

	size_t chunkSize = request_.size() - writeOffset_;
	ssize_t writeCount = socket_.write(request_.data() + writeOffset_, chunkSize);

	if (writeCount < 0) {
		TRACE("write failed. %s", strerror(errno));
		logFailure();
	} else {
		writeOffset_ += writeCount;

		if (writeOffset_ == request_.size()) {
			socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(this, backend_->director()->readTimeout());
			socket_.setMode(Socket::Read);
		}
	}
}

/**
 * Reads and processes a response chunk from origin server.
 */
void HttpHealthMonitor::readSome()
{
	TRACE("readSome()");

	size_t lower_bound = response_.size();
	if (lower_bound == response_.capacity())
		response_.setCapacity(lower_bound + 4096);

	ssize_t rv = socket_.read(response_);

	if (rv > 0) {
		TRACE("readSome: read %zi bytes", rv);
		size_t np = process(response_.ref(lower_bound, rv));

		(void) np;
		TRACE("readSome(): processed %ld of %ld bytes (%s)", np, rv, HttpMessageProcessor::state_str());

		if (HttpMessageProcessor::state() == HttpMessageProcessor::SYNTAX_ERROR) {
			TRACE("syntax error");
			logFailure();
		} else if (processingDone_) {
			TRACE("processing done");
			logSuccess();
		} else {
			TRACE("resume with io:%d, state:%s", socket_.mode(), state_str().c_str());
			socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(this, backend_->director()->readTimeout());
			socket_.setMode(Socket::Read);
		}
	} else if (rv == 0) {
		TRACE("remote endpoint closed connection.");
	} else {
		switch (errno) {
			case EAGAIN:
			case EINTR:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
			case EWOULDBLOCK:
#endif
				break;
			default:
				TRACE("error reading health-check response from backend. %s", strerror(errno));
				logFailure();
				return;
		}
	}
}

/**
 * Origin server timed out in read or write operation.
 */
void HttpHealthMonitor::onTimeout(x0::Socket* s)
{
	TRACE("onTimeout()");
	logFailure();
}

