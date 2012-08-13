#pragma once

#include "HealthMonitor.h"
#include "FastCgiProtocol.h"
#include <x0/Buffer.h>
#include <x0/Socket.h>
#include <x0/http/HttpStatus.h>
#include <ev++.h>


/**
 * HTTP Health Monitor.
 */
class FastCgiHealthMonitor :
	public HealthMonitor
{
public:
	explicit FastCgiHealthMonitor(x0::HttpWorker& worker);
	~FastCgiHealthMonitor();

	virtual void setRequest(const char* fmt, ...);
	virtual void reset();
	virtual void onCheckStart();

private:
	template<typename T, typename... Args>
	void write(Args&&... args);
	void write(FastCgi::Type type, const x0::Buffer& buffer);

	void onConnectDone(x0::Socket*, int revents);
	void io(x0::Socket*, int revents);
	bool writeSome();
	bool readSome();
	void onTimeout(x0::Socket* s);

	bool processRecord(const FastCgi::Record *record);
	void onStdOut(const x0::BufferRef& chunk);
	void onStdErr(const x0::BufferRef& chunk);
	void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

private:
	std::unordered_map<std::string, std::string> fcgiParams_;

	x0::Socket socket_;

	x0::Buffer writeBuffer_;
	size_t writeOffset_;

	x0::Buffer readBuffer_;
	size_t readOffset_;

	x0::HttpStatus responseStatus_;
};
