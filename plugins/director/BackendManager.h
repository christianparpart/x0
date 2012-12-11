/* <x0/plugins/BackendManager.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include <x0/http/HttpWorker.h>
#include <x0/Logging.h>
#include <x0/TimeSpan.h>
#include <string>

namespace x0 {
	class HttpRequest;
	class Url;
}

class Backend;

/** common abstraction of what a backend has to know about its managing owner.
 *
 * \see Director
 */
class BackendManager
#ifndef NDEBUG
	: public Logging
#endif
{
protected:
	x0::HttpWorker* worker_;
	std::string name_;
	x0::TimeSpan connectTimeout_;
	x0::TimeSpan readTimeout_;
	x0::TimeSpan writeTimeout_;

public:
	BackendManager(x0::HttpWorker* worker, const std::string& name);
	virtual ~BackendManager();

	x0::HttpWorker* worker() const { return worker_; }
	const std::string name() const { return name_; }

	x0::TimeSpan connectTimeout() const { return connectTimeout_; }
	void setConnectTimeout(x0::TimeSpan value) { connectTimeout_ = value; }

	x0::TimeSpan readTimeout() const { return readTimeout_; }
	void setReadTimeout(x0::TimeSpan value) { readTimeout_ = value; }

	x0::TimeSpan writeTimeout() const { return writeTimeout_; }
	void setWriteTimeout(x0::TimeSpan value) { writeTimeout_ = value; }

	template<typename T> inline void post(T function) { worker()->post(function); }

	//! Invoked internally when the passed request failed processing.
	virtual void reject(x0::HttpRequest* r) = 0;

	//! Invoked internally when a request has been fully processed in success.
	virtual void release(Backend* backend) = 0;
};
