// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "ClientAbortAction.h"
#include <x0/http/HttpWorker.h>
#include <x0/http/HttpStatus.h>
#include <x0/Counter.h>
#include <x0/Logging.h>
#include <x0/TimeSpan.h>
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
#ifndef XZERO_NDEBUG
    : public x0::Logging
#endif
      {
 protected:
  x0::HttpWorker* worker_;
  std::string name_;
  x0::TimeSpan connectTimeout_;
  x0::TimeSpan readTimeout_;
  x0::TimeSpan writeTimeout_;
  ClientAbortAction clientAbortAction_;
  x0::Counter load_;

  friend class Backend;

 public:
  BackendManager(x0::HttpWorker* worker, const std::string& name);
  virtual ~BackendManager();

  void log(x0::LogMessage&& msg);

  x0::HttpWorker* worker() const { return worker_; }
  const std::string name() const { return name_; }

  x0::TimeSpan connectTimeout() const { return connectTimeout_; }
  void setConnectTimeout(x0::TimeSpan value) { connectTimeout_ = value; }

  x0::TimeSpan readTimeout() const { return readTimeout_; }
  void setReadTimeout(x0::TimeSpan value) { readTimeout_ = value; }

  x0::TimeSpan writeTimeout() const { return writeTimeout_; }
  void setWriteTimeout(x0::TimeSpan value) { writeTimeout_ = value; }

  ClientAbortAction clientAbortAction() const { return clientAbortAction_; }
  void setClientAbortAction(ClientAbortAction value) {
    clientAbortAction_ = value;
  }

  const x0::Counter& load() const { return load_; }

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
  virtual void reject(RequestNotes* rn, x0::HttpStatus status) = 0;

  /**
   * Invoked internally when a request has been fully processed in success.
   *
   * @see reject(RequestNotes*)
   */
  virtual void release(RequestNotes* rn) = 0;
};
