#pragma once

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/Logging.h>
#include <x0/Socket.h>
#include <x0/SocketSpec.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpMessageProcessor.h>
#include <ev++.h>

/**
 * Implements HTTP server health monitoring.
 *
 * \note not thread-safe.
 */
class HealthMonitor :
	public x0::Logging,
	public x0::HttpMessageProcessor
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

private:
	Mode mode_;
	x0::HttpWorker* worker_;
	x0::SocketSpec socketSpec_;
	x0::Socket socket_;
	x0::TimeSpan interval_;
	State state_;

	std::function<void(HealthMonitor*)> onStateChange_;

	x0::Buffer request_;
	size_t writeOffset_;
	x0::Buffer response_;
	int responseCode_;
	bool processingDone_;

	int expectCode_;

	ev::timer timer_;

	size_t successThreshold; //!< number of consecutive succeeding responses before marking changing state to *online*.

	size_t failCount_;		//!< total fail count
	size_t successCount_;	//!< consecutive success count
	time_t offlineTime_;	//!< total time this node has been offline

public:
	explicit HealthMonitor(x0::HttpWorker* worker);
	~HealthMonitor();

	Mode mode() const { return mode_; }
	const std::string& mode_str() const;
	void setMode(Mode value);

	State state() const { return state_; }
	void setState(State value);
	const std::string& state_str() const;
	bool isOnline() const { return state_ == State::Online; }

	void onStateChange(const std::function<void(HealthMonitor*)>& callback);

	const x0::SocketSpec& target() const { return socketSpec_; }
	void setTarget(const x0::SocketSpec& value);

	const x0::TimeSpan& interval() const { return interval_; }
	void setInterval(const x0::TimeSpan& value);

	void setRequest(const char* fmt, ...);

	void setExpectCode(int value) { expectCode_ = value; }
	int expectCode() const { return expectCode_; }

	void start();
	void stop();

private:
	void onCheckStart();
	void onConnectDone(x0::Socket*, int revents);
	void io(x0::Socket*, int revents);
	void writeSome();
	void readSome();
	void onTimeout();

	void logSuccess();
	void logFailure();

	void recheck();

	// response (HttpMessageProcessor)
	virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const x0::BufferRef& text);
	virtual bool onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool onMessageContent(const x0::BufferRef& chunk);
	virtual bool onMessageEnd();
};

x0::Buffer& operator<<(x0::Buffer& output, const HealthMonitor& monitor);
