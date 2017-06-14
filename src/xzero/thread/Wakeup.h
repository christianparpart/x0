// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/Api.h>
#include <xzero/Duration.h>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <list>

namespace xzero {

/**
 * Provides a facility to wait for events.
 *
 * While one or more caller are waiting for one event, another
 * caller can cause those waiting callers to be fired.
 */
class XZERO_BASE_API Wakeup {
 public:
  Wakeup();

  /**
   * Block the current thread and wait for the next wakeup event.
   */
  void waitForNextWakeup();

  /**
   * Block the current thread and wait for the first wakeup event (generation 0).
   */
  void waitForFirstWakeup();

  /**
   * Block the current thread and wait for wakeup of generation > given.
   *
   * @param generation the generation number considered <b>old</b>.
   *                   Any generation number bigger this will wakeup the caller.
   */
  void waitForWakeup(long generation);

  void waitFor(Duration timeout);
  void waitFor(Duration timeout, long oldgen);

  /**
   * Increments the generation and invokes all waiters.
   */
  void wakeup();

  /**
   * Registers a @p callback to be invoked when given generation has become old.
   *
   * @param generation Threshold of old generations.
   * @param callback Callback to invoke upon fire.
   */
  void onWakeup(long generation, std::function<void()> callback);

  /**
   * Registers a @p callback to be invoked when current generation has become old.
   */
  void onNextWakeup(std::function<void()> callback);

  /**
   * Retrieves the current wakeup-generation number.
   */
  long generation() const;

 protected:
  std::mutex mutex_;
  std::condition_variable condvar_;
  std::atomic<long> gen_;
  std::list<std::function<void()>> callbacks_;
};

inline void Wakeup::onNextWakeup(std::function<void()> callback) {
  onWakeup(generation(), callback);
}

} // namespace xzero
