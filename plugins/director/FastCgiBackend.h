/* <plugins/director/FastCgiBackend.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include "Backend.h"
#include "FastCgiProtocol.h"

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

class FastCgiBackend :
	public Backend
{
public:
	static std::atomic<uint16_t> nextID_;

public:
	FastCgiBackend(BackendManager* manager, const std::string& name, const x0::SocketSpec& socketSpec, size_t capacity, bool healthChecks);
	~FastCgiBackend();

	void setup(const x0::SocketSpec& spec);

	virtual const std::string& protocol() const;
	virtual bool process(x0::HttpRequest* r);

	void release(FastCgiTransport* transport);

	friend class FastCgiTransport;
};
