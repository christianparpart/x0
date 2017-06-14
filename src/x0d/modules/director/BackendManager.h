// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "ClientAbortAction.h"
#include <xzero/HttpWorker.h>
#include <xzero/HttpStatus.h>
#include <base/Counter.h>
#include <base/Logging.h>
#include <base/Duration.h>
#include <string>

class Backend;
struct RequestNotes;

/**
 * Core interface for a backend manager.
 *
 * Common abstraction of what a backend has to know about its managing owner.
 *
 * \see Director, Roadwarrior
 */
class BackendManager
#ifndef NDEBUG
    : public base::Logging
#endif
      {
 protected:
  xzero::HttpWorker* worker_;
  std::string name_;
  base::Duration connectTimeout_;
  base::Duration readTimeout_;
  base::Duration writeTimeout_;
  ClientAbortAction clientAbortAction_;
  base::Counter load_;

  friend class Backend;

 public:
  BackendManager(xzero::HttpWorker* worker, const std::string& name);
  virtual ~BackendManager();

  void log(base::LogMessage&& msg);

  xzero::HttpWorker* worker() const { return worker_; }
  const std::string name() const { return name_; }

  base::Duration connectTimeout() const { return connectTimeout_; }
  void setConnectTimeout(base::Duration value) { connectTimeout_ = value; }

  base::Duration readTimeout() const { return readTimeout_; }
  void setReadTimeout(base::Duration value) { readTimeout_ = value; }

  base::Duration writeTimeout() const { return writeTimeout_; }
  void setWriteTimeout(base::Duration value) { writeTimeout_ = value; }

  ClientAbortAction clientAbortAction() const { return clientAbortAction_; }
  void setClientAbortAction(ClientAbortAction value) {
    clientAbortAction_ = value;
  }

  const base::Counter& load() const { return load_; }

  template <typename T>
  inline void post(T function) {
    worker()->post(function);
  }

  /**
   * Used to notify the backend manager that the associated backend will has
   *rejected processing this request.
   *
   * The backend manager can put it back to the cluster try rescheduling it to
   *another backend,
   * or send an appropriate response status back to the client, directly
   *terinating this request.
   *
   * @param rn request that was rejected.
   * @param status reject status, reason why this request got rejected.
   *
   * @see release(RequestNotes*)
   * @see Director::schedule()
   * @see Director::reschedule()
   */
  virtual void reject(RequestNotes* rn, xzero::HttpStatus status) = 0;

  /**
   * Invoked internally when a request has been fully processed in success.
   *
   * @see reject(RequestNotes*)
   */
  virtual void release(RequestNotes* rn) = 0;
};
