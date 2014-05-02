/* <plugins/director/FastCgiBackend.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "Backend.h"
#include "FastCgiProtocol.h"

#include <x0/http/HttpMessageParser.h>
#include <x0/Logging.h>
#include <x0/SocketSpec.h>

namespace x0 {
    class HttpServer;
    class HttpRequest;
    class Buffer;
    class Socket;
}

/**
 * @brief implements the handling of one FCGI backend.
 *
 * A FCGI backend may manage multiple transport connections,
 * each either idle, or serving one or more currently active
 * HTTP client requests.
 */
class FastCgiBackend :
    public Backend
{
private:
    class Connection;

public:
    FastCgiBackend(BackendManager* manager, const std::string& name, const x0::SocketSpec& socketSpec, size_t capacity, bool healthChecks);
    ~FastCgiBackend();

    void setup(const x0::SocketSpec& spec);

    const std::string& protocol() const override;
    bool process(RequestNotes* rn) override;

    using Backend::release;
    void release(Connection* transport);
};
