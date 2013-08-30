/* <plugins/director/FastCgiBackend.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
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

/**
 * @brief implements the handling of one FCGI backend.
 *
 * A FCGI backend may manage multiple transport connections,
 * each either idle, or serving one or more currently active
 * HTTP client requests.
 *
 * @see FastCgiTransport
 */
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
	virtual bool process(RequestNotes* rn);

	using Backend::release;
	void release(FastCgiTransport* transport);

	friend class FastCgiTransport;
};
