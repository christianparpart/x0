// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "Backend.h"
#include "FastCgiProtocol.h"

#include <xzero/HttpMessageParser.h>
#include <base/Logging.h>
#include <base/SocketSpec.h>

namespace base {
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
class FastCgiBackend : public Backend {
 private:
  class Connection;

 public:
  FastCgiBackend(BackendManager* manager, const std::string& name,
                 const base::SocketSpec& socketSpec, size_t capacity,
                 bool healthChecks);
  ~FastCgiBackend();

  const std::string& protocol() const override;
  bool process(RequestNotes* rn) override;
};
