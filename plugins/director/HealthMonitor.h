#pragma once

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/Logging.h>
#include <x0/Socket.h>
#include <x0/SocketSpec.h>
#include <x0/http/HttpError.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpMessageProcessor.h>
#include <ev++.h>

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

	enum class State {
		Undefined,
		Offline,
		Online
	};

protected:
	Mode mode_;
	x0::HttpWorker& worker_;
	x0::SocketSpec socketSpec_;
	x0::TimeSpan interval_;
	State state_;

	std::function<void(HealthMonitor*)> onStateChange_;

	x0::HttpError expectCode_;

	ev::timer timer_;

	size_t successThreshold; //!< number of consecutive succeeding responses before marking changing state to *online*.

	size_t failCount_;		//!< total fail count
	size_t successCount_;	//!< consecutive success count
	time_t offlineTime_;	//!< total time this node has been offline

	x0::HttpError responseCode_;
	bool processingDone_;

public:
	explicit HealthMonitor(x0::HttpWorker& worker);
	virtual ~HealthMonitor();

	Mode mode() const { return mode_; }
	const std::string& mode_str() const;
	void setMode(Mode value);

	State state() const { return state_; }
	void setState(State value);
	const std::string& state_str() const;
	bool isOnline() const { return state_ == State::Online; }

	const x0::SocketSpec& target() const { return socketSpec_; }
	void setTarget(const x0::SocketSpec& value);

	const x0::TimeSpan& interval() const { return interval_; }
	void setInterval(const x0::TimeSpan& value);

	void setExpectCode(x0::HttpError value) { expectCode_ = value; }
	x0::HttpError expectCode() const { return expectCode_; }

	void setStateChangeCallback(const std::function<void(HealthMonitor*)>& callback);

	virtual void setRequest(const char* fmt, ...) = 0;
	virtual void reset();

	void start();
	void stop();

protected:
	virtual void onCheckStart();

	void logSuccess();
	void logFailure();

private:
	void recheck();

	// response (HttpMessageProcessor)
	virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const x0::BufferRef& text);
	virtual bool onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool onMessageContent(const x0::BufferRef& chunk);
	virtual bool onMessageEnd();
};

x0::Buffer& operator<<(x0::Buffer& output, const HealthMonitor& monitor);
