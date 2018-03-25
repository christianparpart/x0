// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "BackendManager.h"
#include <base/SocketSpec.h>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace base {
class JsonWriter;
}

class Backend;

/*!
 * Very basic backend-manager, used for simple reverse proxying of HTTP and
 * FastCGI requests.
 */
class RoadWarrior : public BackendManager {
 public:
  enum Protocol { HTTP = 1, FCGI = 2, };

 public:
  explicit RoadWarrior(xzero::HttpWorker* worker);
  ~RoadWarrior();

  void handleRequest(RequestNotes* rn, const base::SocketSpec& spec,
                     Protocol protocol);

  void reject(RequestNotes* rn, xzero::HttpStatus status) override;
  void release(RequestNotes* rn) override;

  void writeJSON(base::JsonWriter& output) const;

 private:
  Backend* acquireBackend(const base::SocketSpec& spec, Protocol protocol);

 private:
  std::mutex backendsLock_;
  std::unordered_map<base::SocketSpec, std::unique_ptr<Backend>> backends_;
};
