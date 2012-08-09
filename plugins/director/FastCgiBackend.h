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

class FastCgiTransport;

class FastCgiBackend : //{{{
	public Backend
{
public:
	static std::atomic<uint16_t> nextID_;

	x0::HttpServer& server_;
	x0::SocketSpec spec_;

public:
	FastCgiBackend(Director* director, const std::string& name, const x0::SocketSpec& socketSpec, size_t capacity);
	~FastCgiBackend();

	x0::HttpServer& server() const { return server_; }

	void setup(const x0::SocketSpec& spec);

	virtual const std::string& protocol() const;
	virtual bool process(x0::HttpRequest* r);

	void release(FastCgiTransport* transport);
};
// }}}
