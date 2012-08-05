#include "HealthMonitor.h"
#include <cassert>
#include <cstdarg>

/*
 * TODO
 * - connect/read/write timeout handling.
 * - proper error reporting.
 * - properly support all modes
 *   - Opportunistic
 *   - Lazy
 */

using namespace x0;

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->debug(msg))
#else
#	define TRACE(msg...) do { } while (0)
#endif

HealthMonitor::HealthMonitor(HttpWorker& worker) :
	Logging("HealthMonitor"),
	HttpMessageProcessor(HttpMessageProcessor::RESPONSE),
	mode_(Mode::Paranoid),
	worker_(worker),
	socketSpec_(),
	interval_(TimeSpan::fromSeconds(2)),
	state_(State::Undefined),
	onStateChange_(),
	expectCode_(HttpError::Ok),
	timer_(worker_.loop()),
	successThreshold(2),
	failCount_(0),
	successCount_(0),
	responseCode_(HttpError::Undefined),
	processingDone_(false)
{
	timer_.set<HealthMonitor, &HealthMonitor::onCheckStart>(this);
}

HealthMonitor::~HealthMonitor()
{
	stop();
}

const std::string& HealthMonitor::mode_str() const
{
	static const std::string modeStr[] = {
		"paranoid", "opportunistic", "lazy"
	};

	return modeStr[static_cast<size_t>(mode_)];
}

/**
 * Sets monitoring mode.
 */
void HealthMonitor::setMode(Mode value)
{
	if (mode_ == value)
		return;

	mode_ = value;
}

const std::string& HealthMonitor::state_str() const
{
	static const std::string stateStr[] = {
		"undefined", "offline", "online"
	};

	return stateStr[static_cast<size_t>(state_)];
}

/**
 * Forces a health-state change.
 */
void HealthMonitor::setState(State value)
{
	assert(value != State::Undefined && "Setting state to Undefined is not allowed.");
	if (state_ == value)
		return;

	state_ = value;

	TRACE("setState: %s", state_str().c_str());

	if (onStateChange_) {
		onStateChange_(this);
	}

	if (state_ == State::Offline) {
		worker_.post<HealthMonitor, &HealthMonitor::start>(this);
	}
}

/**
 * Sets the callback to be invoked on health state changes.
 */
void HealthMonitor::setStateChangeCallback(const std::function<void(HealthMonitor*)>& callback)
{
	onStateChange_ = callback;
}

void HealthMonitor::setTarget(const SocketSpec& value)
{
	socketSpec_ = value;

#ifndef NDEBUG
	setLoggingPrefix("HealthMonitor/%s", socketSpec_.str().c_str());
#endif
}

void HealthMonitor::setInterval(const TimeSpan& value)
{
	interval_ = value;
}

void HealthMonitor::reset()
{
	responseCode_ = HttpError::Undefined;
	processingDone_ = false;
}

/**
 * Starts health-monitoring an HTTP server.
 */
void HealthMonitor::start()
{
	TRACE("start()");

	reset();

	timer_.start(interval_.value(), 0.0);
}

void HealthMonitor::onCheckStart()
{
	// XXX onCheckStart not overridden,
	// XXX so health-check is kind of useless.
}

/**
 * Stops any active timer or health-check operation.
 */
void HealthMonitor::stop()
{
	TRACE("stop()");

	timer_.stop();

	reset();
}

void HealthMonitor::recheck()
{
	TRACE("recheck()");
	start();
}

void HealthMonitor::logSuccess()
{
	++successCount_;

	if (successCount_ >= successThreshold) {
		TRACE("onMessageEnd: successThreshold reached.");
		setState(State::Online);
	}

	recheck();
}

void HealthMonitor::logFailure()
{
	++failCount_;
	successCount_ = 0;

	setState(State::Offline);

	recheck();
}

/**
 * Callback, invoked on successfully parsed response status line.
 */
bool HealthMonitor::onMessageBegin(int versionMajor, int versionMinor, int code, const BufferRef& text)
{
	TRACE("onMessageBegin: (HTTP/%d.%d, %d, '%s')", versionMajor, versionMinor, code, text.str().c_str());

	responseCode_ = static_cast<HttpError>(code);

	return true;
}

/**
 * Callback, invoked on each successfully parsed response header key/value pair.
 */
bool HealthMonitor::onMessageHeader(const BufferRef& name, const BufferRef& value)
{
	// do nothing with response message headers
	return true;
}

/**
 * Callback, invoked on each partially or fully parsed response body chunk.
 */
bool HealthMonitor::onMessageContent(const BufferRef& chunk)
{
	// do nothing with response body chunk
	return true;
}

/**
 * Callback, invoked when the response message has been fully parsed.
 */
bool HealthMonitor::onMessageEnd()
{
	TRACE("onMessageEnd() state:%s", state_str().c_str());
	processingDone_ = true;

	if (responseCode_ == expectCode_) {
		logSuccess();
	} else {
		logFailure();
	}

	// stop processing
	return false;
}

Buffer& operator<<(Buffer& output, const HealthMonitor& monitor)
{
	output
		<< "{"
		<< "\"mode\": \"" << monitor.mode_str() << "\", "
		<< "\"state\": \"" << monitor.state_str() << "\", "
		<< "\"interval\": " << monitor.interval().totalMilliseconds()
		<< "}";

	return output;
}
