// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "Backend.h"
#include "BackendManager.h"
#include "Director.h"
#include "RequestNotes.h"
#include "HealthMonitor.h"

#if !defined(NDEBUG)
#define TRACE(msg...) (this->Logging::debug(msg))
#else
#define TRACE(msg...) \
  do {                \
  } while (0)
#endif

using namespace xzero;
using namespace base;

/**
 * Initializes the backend.
 *
 * \param director The director object to attach this backend to.
 * \param name name of this backend (must be unique within given director's
 *backends).
 * \param socketSpec backend socket spec (hostname + port, or local unix domain
 *path).
 * \param capacity number of requests this backend is capable of handling in
 *parallel.
 * \param healthMonitor specialized health-monitor instanciation, which will be
 *owned by this backend.
 */
Backend::Backend(BackendManager* bm, const std::string& name,
                 const SocketSpec& socketSpec, size_t capacity,
                 std::unique_ptr<HealthMonitor>&& healthMonitor)
    :
#ifndef NDEBUG
      Logging("Backend/%s", name.c_str()),
#endif
      manager_(bm),
      name_(name),
      capacity_(capacity),
      terminateProtection_(false),
      load_(),
      lock_(),
      enabled_(true),
      socketSpec_(socketSpec),
      healthMonitor_(std::move(healthMonitor)),
      enabledCallback_(),
      jsonWriteCallback_() {
  pthread_spin_init(&lock_, PTHREAD_PROCESS_PRIVATE);
}

Backend::~Backend() { pthread_spin_destroy(&lock_); }

void Backend::log(LogMessage&& msg) {
  msg.addTag(name_);
  manager_->log(std::move(msg));
}

size_t Backend::capacity() const { return capacity_; }

void Backend::setCapacity(size_t value) { capacity_ = value; }

void Backend::writeJSON(JsonWriter& json) const {
  json.beginObject()
      .name("name")(name_)
      .name("capacity")(capacity_)
      .name("terminate-protection")(terminateProtection_)
      .name("enabled")(enabled_)
      .name("protocol")(protocol());

  if (socketSpec_.isInet()) {
    json.name("hostname")(socketSpec_.ipaddr().str())
        .name("port")(socketSpec_.port());
  } else {
    json.name("path")(socketSpec_.local());
  }

  json.beginObject("stats");
  json.name("load")(load_);
  json.endObject();

  if (healthMonitor_) {
    json.name("health")(*healthMonitor_);
  }

  if (jsonWriteCallback_) jsonWriteCallback_(this, json);

  json.endObject();
}

void Backend::setEnabled(bool value) {
  if (enabled_ == value) return;

  enabled_ = value;

  if (!enabledCallback_) return;

  enabledCallback_(this);
}

void Backend::setEnabledCallback(
    const std::function<void(const Backend*)>& callback) {
  enabledCallback_ = callback;
}

void Backend::setJsonWriteCallback(
    const std::function<void(const Backend*, JsonWriter&)>& callback) {
  jsonWriteCallback_ = callback;
}

void Backend::setState(HealthState value) {
  if (healthMonitor_) {
    healthMonitor_->setState(value);
  }
}

/*!
 * Tries to processes given request on this backend.
 *
 * \param rn the request to be processed
 *
 * It only processes the request if this backend is healthy, enabled and the
 *load has not yet reached its capacity.
 * It then passes the request to the implementation specific \c process()
 *method. If this fails to initiate
 * processing, this backend gets flagged as offline automatically, otherwise the
 *load counters are increased accordingly.
 *
 * \note <b>MUST</b> be invoked from within the request's worker thread.
 */
SchedulerStatus Backend::tryProcess(RequestNotes* rn) {
  SchedulerStatus status = SchedulerStatus::Unavailable;
  pthread_spin_lock(&lock_);

  if (healthMonitor_ && !healthMonitor_->isOnline()) goto done;

  if (!isEnabled()) goto done;

  status = SchedulerStatus::Overloaded;
  if (capacity_ && load_.current() >= capacity_) goto done;

  rn->request->log(Severity::trace,
                   "Processing request by director '%s' backend '%s'.",
                   manager()->name().c_str(), name().c_str());

  ++load_;
  ++manager_->load_;

  rn->backend = this;
  rn->request->responseHeaders.overwrite("X-Director-Backend", name());

  if (!process(rn)) {
    setState(HealthState::Offline);
    rn->backend = nullptr;
    --manager_->load_;
    --load_;
    status = SchedulerStatus::Unavailable;
    goto done;
  }

  status = SchedulerStatus::Success;

done:
  pthread_spin_unlock(&lock_);
  return status;
}

/**
 * Invoked internally when a request has been fully processed.
 *
 * This decrements the load-statistics, and potentially
 * dequeues possibly enqueued requests to take over.
 */
void Backend::release(RequestNotes* rn) {
  --load_;
  manager_->release(rn);
}

/**
 * Invoked internally when this backend could not handle this request.
 *
 * This decrements the load-statistics and potentially
 * reschedules the request.
 */
void Backend::reject(RequestNotes* rn, HttpStatus status) {
  --load_;

  // and set the backend's health state to offline, since it
  // doesn't seem to function properly
  setState(HealthState::Offline);

  manager_->reject(rn, status);
}
