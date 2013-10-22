/* <plugin/director/HealthMonitor.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/Logging.h>
#include <x0/Socket.h>
#include <x0/SocketSpec.h>
#include <x0/JsonWriter.h>
#include <x0/http/HttpStatus.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpMessageProcessor.h>
#include <ev++.h>

class Backend;

enum class HealthState {
	Undefined,
	Offline,
	Online
};

inline std::string stringify(HealthState value)
{
	static const std::string strings[] = { "Undefined", "Offline", "Online" };
	return strings[static_cast<size_t>(value)];
}

/**
 * Implements HTTP server health monitoring.
 *
 * \note not thread-safe.
 */
class HealthMonitor :
	protected x0::Logging,
	protected x0::HttpMessageProcessor
{
public:
	enum class Mode {
		Paranoid,
		Opportunistic,
		Lazy
	};

protected:
	Mode mode_;
	Backend* backend_;
	x0::HttpWorker& worker_;
	x0::TimeSpan interval_;
	HealthState state_;

	std::function<void(HealthMonitor*, HealthState)> onStateChange_;

	x0::HttpStatus expectCode_;

	ev::timer timer_;

	size_t successThreshold; //!< number of consecutive succeeding responses before marking changing state to *online*.

	size_t failCount_;		//!< total fail count
	size_t successCount_;	//!< consecutive success count
	time_t offlineTime_;	//!< total time this node has been offline

	x0::HttpStatus responseCode_;
	bool processingDone_;

public:
	explicit HealthMonitor(x0::HttpWorker& worker, HttpMessageProcessor::ParseMode parseMode = HttpMessageProcessor::RESPONSE);
	virtual ~HealthMonitor();

	Mode mode() const { return mode_; }
	const std::string& mode_str() const;
	void setMode(Mode value);

	HealthState state() const { return state_; }
	void setState(HealthState value);
	const std::string& state_str() const;
	bool isOnline() const { return state_ == HealthState::Online; }

	Backend* backend() const { return backend_; }
	void setBackend(Backend* backend);

	void update();

	const x0::TimeSpan& interval() const { return interval_; }
	void setInterval(const x0::TimeSpan& value);

	void setExpectCode(x0::HttpStatus value) { expectCode_ = value; }
	x0::HttpStatus expectCode() const { return expectCode_; }

	void setStateChangeCallback(const std::function<void(HealthMonitor*, HealthState)>& callback);

	virtual void setRequest(const char* fmt, ...) = 0;
	virtual void reset();

	void start();
	void stop();

	template<typename T> inline void post(T function) { worker_.post(function); }

protected:
	virtual void onCheckStart();

	void logSuccess();
	void logFailure();

	void recheck();

	// response (HttpMessageProcessor)
	virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const x0::BufferRef& text);
	virtual bool onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool onMessageContent(const x0::BufferRef& chunk);
	virtual bool onMessageEnd();

	virtual void log(x0::LogMessage&& msg);
};

x0::JsonWriter& operator<<(x0::JsonWriter& json, const HealthMonitor& monitor);
