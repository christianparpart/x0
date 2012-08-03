#pragma once

#include "Backend.h"
#include "fastcgi_protocol.h"

#include <x0/http/HttpMessageProcessor.h>
#include <x0/Logging.h>
#include <x0/SocketSpec.h>

namespace x0 {
	class HttpServer;
	class HttpRequest;
	class Buffer;
	class Socket;
}

class CgiTransport;

class FastCgiBackend : //{{{
	public Backend
{
public:
	static std::atomic<uint16_t> nextID_;

	x0::HttpServer& server_;
	x0::SocketSpec spec_;

public:
	FastCgiBackend(Director* director, const std::string& name, size_t capacity, const std::string& hostname, int port);
	~FastCgiBackend();

	x0::HttpServer& server() const { return server_; }

	void setup(const x0::SocketSpec& spec);

	virtual bool process(x0::HttpRequest* r);
	virtual size_t writeJSON(x0::Buffer& output) const;

	void release(CgiTransport *transport);
};
// }}}

class CgiTransport : // {{{
#ifndef NDEBUG
	public x0::Logging,
#endif
	public x0::HttpMessageProcessor
{
	class ParamReader : public FastCgi::CgiParamStreamReader //{{{
	{
	private:
		CgiTransport *tx_;

	public:
		explicit ParamReader(CgiTransport *tx) : tx_(tx) {}

		virtual void onParam(const char *nameBuf, size_t nameLen, const char *valueBuf, size_t valueLen)
		{
			std::string name(nameBuf, nameLen);
			std::string value(valueBuf, valueLen);

			tx_->onParam(name, value);
		}
	}; //}}}
public:
	int refCount_;
	bool isAborted_; //!< just for debugging right now.
	FastCgiBackend *backend_;

	uint16_t id_;
	std::string backendName_;
	x0::Socket* socket_;

	x0::Buffer readBuffer_;
	size_t readOffset_;
	x0::Buffer writeBuffer_;
	size_t writeOffset_;
	bool flushPending_;

	bool configured_;

	// aka CgiRequest
	x0::HttpRequest *request_;
	FastCgi::CgiParamStreamWriter paramWriter_;

	/*! number of write chunks written within a single io() callback. */
	int writeCount_;

public:
	explicit CgiTransport(FastCgiBackend *cx);
	~CgiTransport();

	void ref();
	void unref();

	void bind(x0::HttpRequest *r, uint16_t id, x0::Socket* backend);
	void close();

	// server-to-application
	void abortRequest();

	// application-to-server
	void onStdOut(const x0::BufferRef& chunk);
	void onStdErr(const x0::BufferRef& chunk);
	void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

	FastCgiBackend& backend() const { return *backend_; }

public:
	template<typename T, typename... Args> void write(Args&&... args);
	void write(FastCgi::Type type, int requestId, x0::Buffer&& content);
	void write(FastCgi::Type type, int requestId, const char *buf, size_t len);
	void write(FastCgi::Record *record);
	void flush();

private:
	void processRequestBody(const x0::BufferRef& chunk);

	virtual bool onMessageHeader(const x0::BufferRef& name, const x0::BufferRef& value);
	virtual bool onMessageContent(const x0::BufferRef& content);

	void onWriteComplete();
	static void onClientAbort(void *p);

	void onConnectComplete(x0::Socket* s, int revents);
	void onConnectTimeout(x0::Socket* s);

	void io(x0::Socket* s, int revents);
	void timeout(x0::Socket* s);

	inline bool processRecord(const FastCgi::Record *record);
	void onParam(const std::string& name, const std::string& value);

	void inspect(x0::Buffer& out);
}; // }}}

