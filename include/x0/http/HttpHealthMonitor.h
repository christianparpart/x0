#pragma once

#include <x0/Api.h>
#include <x0/TimeSpan.h>
#include <x0/Logging.h>
#include <x0/Socket.h>
#include <x0/SocketSpec.h>
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpMessageProcessor.h>
#include <ev++.h>

namespace x0 {

/**
 * Implements HTTP server health monitoring.
 *
 * \note not thread-safe.
 */
class X0_API HttpHealthMonitor :
	public Logging,
	public HttpMessageProcessor
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
	HttpWorker* worker_;
	SocketSpec socketSpec_;
	Socket socket_;
	TimeSpan interval_;
	State state_;

	std::function<void(HttpHealthMonitor*)> onStateChange_;

	Buffer request_;
	size_t writeOffset_;
	Buffer response_;
	int responseCode_;
	bool processingDone_;

	int expectCode_;

	ev::timer timer_;

	size_t successThreshold; //!< number of consecutive succeeding responses before marking changing state to *online*.

	size_t failCount_;		//!< total fail count
	size_t successCount_;	//!< consecutive success count
	time_t offlineTime_;	//!< total time this node has been offline

public:
	explicit HttpHealthMonitor(HttpWorker* worker);
	~HttpHealthMonitor();

	Mode mode() const { return mode_; }
	const std::string& mode_str() const;
	void setMode(Mode value);

	State state() const { return state_; }
	void setState(State value);
	const std::string& state_str() const;
	bool isOnline() const { return state_ == State::Online; }

	void onStateChange(const std::function<void(HttpHealthMonitor*)>& callback);

	const SocketSpec& target() const { return socketSpec_; }
	void setTarget(const SocketSpec& value);

	const TimeSpan& interval() const { return interval_; }
	void setInterval(const TimeSpan& value);

	void setRequest(const char* fmt, ...);

	void setExpectCode(int value) { expectCode_ = value; }
	int expectCode() const { return expectCode_; }

	void start();
	void stop();

private:
	void onCheckStart();
	void onConnectDone(Socket*, int revents);
	void io(Socket*, int revents);
	void writeSome();
	void readSome();
	void onTimeout();

	void logSuccess();
	void logFailure();

	void recheck();

	// response (HttpMessageProcessor)
	virtual bool onMessageBegin(int versionMajor, int versionMinor, int code, const BufferRef& text);
	virtual bool onMessageHeader(const BufferRef& name, const BufferRef& value);
	virtual bool onMessageContent(const BufferRef& chunk);
	virtual bool onMessageEnd();
};

} // namespace x0
