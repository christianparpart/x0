// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "RoadWarrior.h"
#include "Backend.h"
#include "RequestNotes.h"
#include "HttpBackend.h"
#include "FastCgiBackend.h"
#include <xzero/HttpRequest.h>

using namespace base;
using namespace xzero;

RoadWarrior::RoadWarrior(HttpWorker* worker)
    : BackendManager(worker, "__roadwarrior__"), backendsLock_(), backends_() {}

RoadWarrior::~RoadWarrior() {}

Backend* RoadWarrior::acquireBackend(const SocketSpec& spec,
                                     Protocol protocol) {
  std::lock_guard<std::mutex> _l(backendsLock_);

  auto bi = backends_.find(spec);
  if (bi != backends_.end()) {
    return bi->second.get();
  }

  Backend* backend = nullptr;
  switch (protocol) {
    case HTTP:
      backends_[spec].reset(
          backend = new HttpBackend(this, spec.str(), spec, 0, false));
      break;
    case FCGI:
      backends_[spec].reset(
          backend = new FastCgiBackend(this, spec.str(), spec, 0, false));
      break;
  }
  return backend;
}

void RoadWarrior::handleRequest(RequestNotes* rn, const SocketSpec& spec,
                                Protocol protocol) {
  Backend* backend = acquireBackend(spec, protocol);
  if (!backend) {
    rn->request->status = HttpStatus::InternalServerError;
    rn->request->finish();
  }

  SchedulerStatus result = backend->tryProcess(rn);
  if (result != SchedulerStatus::Success) {
    rn->request->status = HttpStatus::ServiceUnavailable;
    rn->request->finish();
  }
}

void RoadWarrior::reject(RequestNotes* rn, HttpStatus status) {
  // this request couldn't be served by the backend, so finish it with a 503
  // (Service Unavailable).

  auto r = rn->request;

  r->status = status;

  r->finish();
}

void RoadWarrior::release(RequestNotes* rn) {
  // The passed backend just finished serving a request, so we might now pass it
  // a queued request,
  // in case we would support queuing (do we want that?).
}

void RoadWarrior::writeJSON(JsonWriter& json) const {
  json.beginObject(name());
  json.beginArray("members");
  for (const auto& backend : backends_) {
    json.value(*backend.second);
  }
  json.endArray();
  json.endObject();
}
