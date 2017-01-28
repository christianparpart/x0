// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "HealthMonitor.h"
#include "SchedulerStatus.h"

#include <base/Counter.h>
#include <base/Logging.h>
#include <base/Duration.h>
#include <base/SocketSpec.h>
#include <base/JsonWriter.h>
#include <base/CustomDataMgr.h>
#include <memory>
#include <pthread.h>

class BackendManager;
class Director;
struct RequestNotes;

/*!
 * \brief abstract base class for the actual proxying instances as used by \c
 *BackendManager.
 *
 * \see BackendManager, HttpBackend, FastCgiBackend
 */
class Backend
#ifndef NDEBUG
    : public base::Logging
#endif
      {
  CUSTOMDATA_API_INLINE

 protected:
  BackendManager* manager_;  //!< manager, this backend is registered to

  std::string name_;  //!< common name of this backend, for example:
                      //"appserver05"
  size_t capacity_;   //!< number of concurrent requests being processable at a
                      //time.
  bool terminateProtection_;  //!< termination-protected
  base::Counter load_;          //!< number of active (busy) connections

  pthread_spinlock_t lock_;  //!< scheduling mutex

  bool enabled_;  //!< whether or not this backend is enabled (default) or
                  //disabled (for example for maintenance reasons)
  base::SocketSpec socketSpec_;                     //!< Backend socket spec.
  std::unique_ptr<HealthMonitor> healthMonitor_;  //!< health check timer

  std::function<void(const Backend*)> enabledCallback_;
  std::function<void(const Backend*, base::JsonWriter&)> jsonWriteCallback_;

  friend class Director;

 public:
  Backend(BackendManager* bm, const std::string& name,
          const base::SocketSpec& socketSpec, size_t capacity,
          std::unique_ptr<HealthMonitor>&& healthMonitor);
  virtual ~Backend();

  void log(base::LogMessage&& msg);

  void setEnabledCallback(const std::function<void(const Backend*)>& callback);
  void setJsonWriteCallback(
      const std::function<void(const Backend*, base::JsonWriter&)>& callback);

  virtual const std::string& protocol() const = 0;

  const std::string& name() const {
    return name_;
  }  //!< descriptive name of backend.
  BackendManager* manager() const {
    return manager_;
  }  //!< manager instance that owns this backend.

  size_t capacity() const;  //!< number of requests this backend can handle in
                            //parallel.
  void setCapacity(size_t value);

  bool terminateProtection() const { return terminateProtection_; }
  void setTerminateProtection(bool value) { terminateProtection_ = value; }

  const base::Counter& load() const {
    return load_;
  }  //!< number of currently being processed requests.

  const base::SocketSpec& socketSpec() const {
    return socketSpec_;
  }  //!< retrieves the backend socket spec

  // enable/disable state
  void enable() { setEnabled(true); }
  bool isEnabled() const { return enabled_; }
  void setEnabled(bool value);
  void disable() { setEnabled(false); }

  // health monitoring
  HealthMonitor* healthMonitor() { return healthMonitor_.get(); }
  const HealthMonitor* healthMonitor() const { return healthMonitor_.get(); }
  HealthState healthState() const { return healthMonitor_->state(); }

  SchedulerStatus tryProcess(RequestNotes* rn);
  void release(RequestNotes* rn);
  void reject(RequestNotes* rn, xzero::HttpStatus status);

  virtual void writeJSON(base::JsonWriter& json) const;

 protected:
  /*!
   * \brief initiates actual processing of given request.
   *
   * \note this method MUST NOT block.
   */
  virtual bool process(RequestNotes* rn) = 0;

  //	friend class Scheduler;
  //	friend class LeastLoadScheduler;
  //	friend class ClassfulScheduler;

 protected:
  void setState(HealthState value);
};

inline base::JsonWriter& operator<<(base::JsonWriter& json,
                                    const Backend& backend) {
  backend.writeJSON(json);
  return json;
}
