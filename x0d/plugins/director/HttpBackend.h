/* <plugins/director/HttpBackend.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "Backend.h"

/*!
 * implements the HTTP backend.
 *
 * \see FastCgiBackend
 */
class HttpBackend : public Backend {
private:
    class Connection;

public:
    HttpBackend(BackendManager* director, const std::string& name, const x0::SocketSpec& socketSpec, size_t capacity, bool healthChecks);
    ~HttpBackend();

    const std::string& protocol() const override;
    bool process(RequestNotes* rn) override;

private:
    Connection* acquireConnection();
};


