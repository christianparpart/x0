// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Duration.h>
#include <xzero/Counter.h>
#include <xzero/JsonWriter.h>
#include <xzero/MonotonicTime.h>
#include <xzero/MonotonicClock.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>

#include <functional>
#include <algorithm>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <stdint.h>
#include <cassert>
#include <mutex>

namespace xzero {

#define TS_TRACE(msg...) logTrace("TokenShaper", msg)

/**
 * TokenShaper mutation result codes.
 */
enum class TokenShaperError {
  // Operation has completed successfully.
  Success,

  // Operation failed as rate limit is either too low or too high.
  RateLimitOverflow,

  // Operation failed as ceil limit is either too low or too high.
  CeilLimitOverflow,

  // Operation failed as given name already exists somewhere else in the tree.
  NameConflict,

  // Operation failed as this node must not be the root node for the operation to complete.
  InvalidChildNode,
};

/**
 * Queuing bucket, inspired by the HTB algorithm, that's being used in Linux
 * traffic shaping.
 *
 * Features:
 * <ul>
 *   <li> Hierarichal Token-based Asynchronous Scheduling </li>
 *   <li> Node-Level Queuing and fair round-robin inter-node dequeuing.</li>
 *   <li> Queue Timeout Management </li>
 * </ul>
 *
 * Since this shaper just ensures whether or not to directly run the task with
 * the given token or not, and if not, it also enqueues it,
 * you have to actually *run* your task that's being associated with the given
 * token cost.
 *
 * When you have successfully allocated the token(s) for your task with get(),
 * you have to explicitely free them up when your task with put() has finished.
 *
 * Rate and ceiling margins are configured in percentage relative to their
 * parent node. The root node's rate and ceil though must be 100%
 * and cannot be changed.
 *
 * Updating rate/ceiling for non-root nodes will trigger a recursive update
 * for all its descendants.
 *
 * Updating the root node's token capacity will trigger a recursive
 * recomputation of the descendant node's token rate and token ceil values.
 *
 * <h2> Node Properties </h2>
 *
 * <ul>
 * 	<li> assured rate (AR) - The rate that is assued to this node. </li>
 * 	<li> ceil rate (CR) - The rate that must not be exceeded. </li>
 * 	<li> actual rate (R) - The actual rate of tokens (or packets, ...) that has
 *       been acquired through this node. </li>
 * 	<li> over rate (OR) - The rate between the AR and CR. </li>
 * </ul>
 *
 * @note Not threadsafe.
 *
 * @see http://luxik.cdi.cz/~devik/qos/htb/manual/theory.htm
 */
template <typename T>
class TokenShaper {
 public:
  class Node;
  typedef std::function<void(T*)> TimeoutHandler;

  TokenShaper(Executor* executor,
              size_t size,
              std::function<void(T*)> timeoutHandler);
  ~TokenShaper();

  Executor* executor() const { return root_->executor_; }

  size_t size() const;
  void resize(size_t capacity);

  Node* rootNode() const;
  Node* findNode(const std::string& name) const;
  TokenShaperError createNode(const std::string& name, float rate,
                              float ceil = 0);
  void destroyNode(Node* n);

  size_t get(size_t tokens) { return root_->get(tokens); }
  void put(size_t tokens) { return root_->put(tokens); }
  T* dequeue() { return root_->dequeue(); }

  void writeJSON(JsonWriter& json) const;

 private:
  Node* root_;
};

template <typename T>
class TokenShaper<T>::Node {
 public:
  //! @todo must be thread safe to allow bucket iteration while modification
  typedef std::vector<TokenShaper<T>::Node*> BucketList;
  typedef typename BucketList::const_iterator const_iterator;

  // user attributes
  const std::string& name() const noexcept;
  float rateP() const noexcept;
  float ceilP() const noexcept;

  TokenShaperError setName(const std::string& value);
  TokenShaperError setRate(float value);
  TokenShaperError setCeil(float value);
  TokenShaperError setRate(float assuredRate, float ceilRate);

  size_t rate() const noexcept;
  size_t ceil() const noexcept;

  size_t actualRate() const;
  size_t overRate() const;

  // child rates
  float childRateP() const;
  size_t childRate() const;
  size_t actualChildRate() const;
  size_t actualChildOverRate() const;

  // parent/child node access
  static TokenShaper<T>::Node* createRoot(Executor* executor, size_t tokens,
                                          TimeoutHandler onTimeout);
  TokenShaperError createChild(const std::string& name, float rate,
                               float ceil = 0);
  TokenShaper<T>::Node* findChild(const std::string& name) const;
  TokenShaper<T>::Node* rootNode();
  void destroyChild(Node* n);

  TokenShaper<T>::Node& operator[](const std::string& name) const;

  Node* parentNode() const noexcept;

  bool send(T* packet, size_t cost = 1);
  size_t get(size_t tokens);
  void put(size_t tokens);

  void enqueue(T* value);
  T* dequeue();

  const Counter& queued() const noexcept;
  unsigned long long dropped() const;

  Duration queueTimeout() const;
  void setQueueTimeout(Duration value);

  // child bucket access
  bool empty() const;
  size_t size() const;
  const_iterator begin() const;
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

  void writeJSON(JsonWriter& json) const;

 private:
  friend class TokenShaper;
  void update(size_t n);
  void update();
  void updateQueueTimer();
  void onTimeout();

 private:
  Node(Executor* executor, const std::string& name, size_t tokenRate,
       size_t tokenCeil, float prate, float pceil,
       TokenShaper<T>::Node* parent,
       TimeoutHandler onTimeout);

  struct QueueItem {
    T* token;
    MonotonicTime ctime;

    QueueItem() {}
    QueueItem(const QueueItem&) = default;
    QueueItem(T* _token, MonotonicTime _ctime) : token(_token), ctime(_ctime) {}
  };

  /** used for queue timeout management. */
  Executor* executor_;

  /** bucket name */
  std::string name_;

  /** maximum tokens this bucket and all its children are guaranteed. */
  std::atomic<size_t> rate_;

  /** maximum tokens this bucket can send if parent has enough tokens spare. */
  std::atomic<size_t> ceil_;

  float ratePercent_;   //!< rate in percent relative to parent's ceil
  float ceilPercent_;   //!< ceil in percent relative to parent's ceil

  Node* parent_;        //!< parent bucket this bucket is a direct child of
  BucketList children_; //!< direct child buckets

  Counter actualRate_;  //!< bucket load stats
  Counter queued_;      //!< bucket queue stats

  /** Number of tokens dropped due to queue timeouts. */
  std::atomic<unsigned long long> dropped_;

  /** time span on how long a token may stay in queue. */
  Duration queueTimeout_;

  /** FIFO queue of tokens that could not be passed directly. */
  std::deque<QueueItem> queue_;

  /** dequeue-offset at which child to dequeue next. */
  size_t dequeueOffset_;

  /** Callback, invoked when the token has been queued and just timed out. */
  TimeoutHandler onTimeout_;

  std::mutex lock_;
};

/**
 * Dumps given TokenShaper to stdout.
 */
template <class T>
void dump(const TokenShaper<T>& shaper, const char* title);

}  // namespace xzero

#include <xzero/TokenShaper-inl.h>
