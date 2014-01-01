/* <x0/plugins/BackendManager.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/http/HttpWorker.h>
#include <x0/Counter.h>
#include <x0/Logging.h>
#include <x0/TimeSpan.h>
#include <string>

class Backend;
struct RequestNotes;

enum class TransferMode {
	Blocking = 0,
	MemoryAccel = 1,
	FileAccel = 2,
};

TransferMode makeTransferMode(const std::string& value);
std::string tos(TransferMode value);

/**
 * Core interface for a backend manager.
 *
 * Common abstraction of what a backend has to know about its managing owner.
 *
 * \see Director, Roadwarrior
 */
class BackendManager
#ifndef XZERO_NDEBUG
	: public x0::Logging
#endif
{
protected:
	x0::HttpWorker* worker_;
	std::string name_;
	x0::TimeSpan connectTimeout_;
	x0::TimeSpan readTimeout_;
	x0::TimeSpan writeTimeout_;
	TransferMode transferMode_;		//!< Mode how response payload is transferred.
	x0::Counter load_;

	friend class Backend;

public:
	BackendManager(x0::HttpWorker* worker, const std::string& name);
	virtual ~BackendManager();

	void log(x0::LogMessage&& msg);

	x0::HttpWorker* worker() const { return worker_; }
	const std::string name() const { return name_; }

	x0::TimeSpan connectTimeout() const { return connectTimeout_; }
	void setConnectTimeout(x0::TimeSpan value) { connectTimeout_ = value; }

	x0::TimeSpan readTimeout() const { return readTimeout_; }
	void setReadTimeout(x0::TimeSpan value) { readTimeout_ = value; }

	x0::TimeSpan writeTimeout() const { return writeTimeout_; }
	void setWriteTimeout(x0::TimeSpan value) { writeTimeout_ = value; }

	TransferMode transferMode() const { return transferMode_; }
	void setTransferMode(TransferMode value) { transferMode_ = value; }

	const x0::Counter& load() const { return load_; }

	template<typename T> inline void post(T function) { worker()->post(function); }

	//! Invoked internally when the passed request failed processing.
	virtual void reject(RequestNotes* rn) = 0;

	//! Invoked internally when a request has been fully processed in success.
	virtual void release(RequestNotes* rn) = 0;
};

namespace x0 {
	class JsonWriter;
	JsonWriter& operator<<(JsonWriter& json, const TransferMode& mode);
}

