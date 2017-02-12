// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/AnsiColor.h>
#include <stdio.h>

namespace xzero {

// {{{ TokenShaper<T> impl
template <typename T>
TokenShaper<T>::TokenShaper(Executor* executor, size_t size,
                            TimeoutHandler timeoutHandler)
    : root_(Node::createRoot(executor, size, timeoutHandler)) {}

template <typename T>
TokenShaper<T>::~TokenShaper() {
  delete root_;
}

template <typename T>
size_t TokenShaper<T>::size() const {
  return root_->rate();
}

template <typename T>
void TokenShaper<T>::resize(size_t capacity) {
  // Only recompute tokenRates on child nodes when root node's token rate
  // actually changed.
  if (root_->rate() == capacity) return;

  root_->update(capacity);
}

template <typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::rootNode() const {
  return root_;
}

template <typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::findNode(
    const std::string& name) const {
  return root_->findChild(name);
}

template <typename T>
TokenShaperError TokenShaper<T>::createNode(const std::string& name, float rate,
                                            float ceil) {
  return root_->createChild(name, rate, ceil);
}

template <typename T>
void TokenShaper<T>::destroyNode(Node* n) {
  if (n == root_) return;

  n->parentNode()->destroyChild(n);
}

template <typename T>
void TokenShaper<T>::writeJSON(JsonWriter& json) const {
  return root_->writeJSON(json);
}
// }}}
// {{{ TokenShaper<T>::Node impl
template <typename T>
TokenShaper<T>::Node::Node(Executor* executor, const std::string& name,
                           size_t tokenRate, size_t tokenCeil, float rate,
                           float ceil, TokenShaper<T>::Node* parent,
                           TimeoutHandler onTimeout)
    : executor_(executor),
      name_(name),
      rate_(tokenRate),
      ceil_(tokenCeil),
      ratePercent_(rate),
      ceilPercent_(ceil),
      parent_(parent),
      children_(),
      actualRate_(),
      queued_(),
      dropped_(0),
      queueTimeout_(50_seconds),
      queue_(),
      dequeueOffset_(0),
      onTimeout_(onTimeout) {
  if (parent_) {
    onTimeout_ = parent_->onTimeout_;
  }
}

template <typename T>
inline const std::string& TokenShaper<T>::Node::name() const noexcept {
  return name_;
}

template <typename T>
inline float TokenShaper<T>::Node::rateP() const noexcept {
  return ratePercent_;
}

template <typename T>
inline float TokenShaper<T>::Node::ceilP() const noexcept {
  return ceilPercent_;
}

template <typename T>
float TokenShaper<T>::Node::childRateP() const {
  float sum = 0;

  for (const auto child : children_) {
    sum += child->rateP();
  }

  return sum;
}

/**
 * Number of tokens reserved by child nodes.
 *
 * This value will be less or equal to this node's computed token-rate.
 */
template <typename T>
size_t TokenShaper<T>::Node::childRate() const {
  size_t sum = 0;

  for (const auto& child : children_) sum += child->rate();

  return sum;
}

/**
 * Number of reserved tokens actually used by its children.
 *
 * \see childRate()
 * \see rate()
 */
template <typename T>
size_t TokenShaper<T>::Node::actualChildRate() const {
  size_t sum = 0;

  for (const auto& child : children_) sum += child->actualRate();

  return sum;
}

template <typename T>
size_t TokenShaper<T>::Node::actualChildOverRate() const {
  size_t sum = 0;

  for (const auto& child : children_) sum += child->overRate();

  return sum;
}

template <typename T>
TokenShaperError TokenShaper<T>::Node::setName(const std::string& value) {
  if (rootNode()->findChild(value)) return TokenShaperError::NameConflict;

  name_ = value;
  return TokenShaperError::Success;
}

template <typename T>
TokenShaperError TokenShaper<T>::Node::setRate(float newRate) {
  if (!parent_) return TokenShaperError::InvalidChildNode;

  if (newRate < 0.0 || newRate > ceilPercent_)
    return TokenShaperError::RateLimitOverflow;

  ratePercent_ = newRate;
  rate_ = parent_->rate() * ratePercent_;

  for (auto child : children_) {
    child->update();
  }

  return TokenShaperError::Success;
}

template <typename T>
TokenShaperError TokenShaper<T>::Node::setCeil(float newCeil) {
  if (!parent_) return TokenShaperError::InvalidChildNode;

  if (newCeil < ratePercent_ || newCeil > 1.0)
    return TokenShaperError::CeilLimitOverflow;

  ceilPercent_ = newCeil;
  ceil_ = parent_->ceil() * ceilPercent_;

  for (auto child : children_) child->update();

  return TokenShaperError::Success;
}

template <typename T>
TokenShaperError TokenShaper<T>::Node::setRate(float newRate, float newCeil) {
  if (!parent_) return TokenShaperError::InvalidChildNode;

  if (newRate < 0.0 || newRate > newCeil)
    return TokenShaperError::RateLimitOverflow;

  if (newCeil > 1.0) return TokenShaperError::CeilLimitOverflow;

  ratePercent_ = newRate;
  ceilPercent_ = newCeil;

  update();

  return TokenShaperError::Success;
}

template <typename T>
inline size_t TokenShaper<T>::Node::rate() const noexcept {
  return rate_;
}

template <typename T>
inline size_t TokenShaper<T>::Node::ceil() const noexcept {
  return ceil_;
}

template <typename T>
inline size_t TokenShaper<T>::Node::actualRate() const {
  return actualRate_.current();
}

template <typename T>
inline size_t TokenShaper<T>::Node::overRate() const {
  return std::max(static_cast<ssize_t>(actualRate() - rate()),
                  static_cast<ssize_t>(0));
}
template <typename T>
void TokenShaper<T>::Node::update(size_t capacity) {
  rate_ = capacity * ratePercent_;
  ceil_ = capacity * ceilPercent_;

  for (auto child : children_) {
    child->update();
  }
}

template <typename T>
void TokenShaper<T>::Node::update() {
  if (parent_) {
    rate_ = parent_->rate() * ratePercent_;
    ceil_ = parent_->ceil() * ceilPercent_;
  }

  for (auto child : children_) {
    child->update();
  }
}

template <typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::Node::createRoot(
    Executor* executor, size_t tokens, TimeoutHandler onTimeout) {
  return new TokenShaper<T>::Node(executor, "root", tokens, tokens,
                                  1.0f, 1.0f, nullptr, onTimeout);
}

template <typename T>
TokenShaperError TokenShaper<T>::Node::createChild(const std::string& name,
                                                   float rate, float ceil) {
  // 0 <= rate <= (1 - childRate)
  if (rate < 0.0 || rate + childRateP() > 1.0)
    return TokenShaperError::RateLimitOverflow;

  // rate <= ceil <= 1.0
  if (ceil < rate || ceil > 1.0) return TokenShaperError::CeilLimitOverflow;

  if (rootNode()->findChild(name)) return TokenShaperError::NameConflict;

  size_t tokenRate = rate_ * rate;
  size_t tokenCeil = ceil_ * ceil;
  TokenShaper<T>::Node* b = new TokenShaper<T>::Node(
      executor_, name, tokenRate, tokenCeil, rate, ceil, this, onTimeout_);
  children_.push_back(b);
  return TokenShaperError::Success;
}

template <typename T>
void TokenShaper<T>::Node::destroyChild(Node* n) {
  auto i = std::find(children_.begin(), children_.end(), n);

  if (i != children_.end()) {
    children_.erase(i);
    delete n;
  }
}

template <typename T>
inline typename TokenShaper<T>::Node& TokenShaper<T>::Node::operator[](
    const std::string& name) const {
  return *findChild(name);
}

template <typename T>
inline typename TokenShaper<T>::Node* TokenShaper<T>::Node::parentNode() const noexcept {
  return parent_;
}

template <typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::Node::rootNode() {
  Node* n = this;

  while (n->parent_ != nullptr)
    n = n->parent_;

  return n;
}

template <typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::Node::findChild(
    const std::string& name) const {
  for (auto n : children_)
    if (n->name() == name) return n;

  for (auto n : children_)
    if (auto b = n->findChild(name)) return b;

  return nullptr;
}

/**
 * Tries to allocate \p cost tokens and returns true or enqueus \p packet
 *otherwise and returns false.
 *
 * \retval true requested number of tokens allocated.
 * \retval false no tokens allocated, packet queued on this bucket.
 */
template <typename T>
bool TokenShaper<T>::Node::send(T* packet, size_t cost) {
  if (get(cost)) return true;

  enqueue(packet);
  return false;
}

/**
 * Allocates up to \p n tokens from this bucket or nothing if allocation failed.
 *
 * A token is ensured if the actual token rate plus \p n is equal or less than
 * the number of non-reserved tokens.
 *
 * The number of non-reserved tokens equals the node's token rate minus the sum
 *of all children's token rate.
 *
 * If the actual token rate plus \p n is below the node's ceiling, we
 * attempt to <b>borrow</b> one from the parent.
 *
 * \return the actual number of allocated tokens, which is either \p n (all
 *requested tokens) or \p 0 if failed.
 */
template <typename T>
size_t TokenShaper<T>::Node::get(size_t n) {
  // Attempt to acquire tokens from the assured token pool.
  for (;;) {
    auto AR = rate();
    auto R = actualRate();
    auto Rc = childRate();
    auto Oc = actualChildOverRate();

    if (std::max(R, Rc + Oc) + n > AR)
      break;

    if (!actualRate_.increment(n, R))
      continue;

    for (Node* p = parent_; p; p = p->parent_)
      p->actualRate_ += n;

    return n;
  }

  // Attempt to borrow tokens from parent if and only if the resulting node's
  // rate does not exceed its ceiling.
  {
    std::lock_guard<std::mutex> _l(lock_);

    if (actualRate() + n <= ceil() && parent_ && parent_->get(n))
      actualRate_ += n;
    else
      n = 0;
  }

  // Acquiring %n tokens failed, so return 0 as indication.
  return n;
}

/*!
 * Puts back \p n tokens into the bucket.
 */
template <typename T>
void TokenShaper<T>::Node::put(size_t n) {
  // you may not refund more tokens than the bucket's ceiling limit.
  assert(n <= actualRate());
  assert(actualChildRate() <= actualRate() - n);

  actualRate_ -= n;

  // Release tokens from parent if the the new actual token rate (load) is still
  // not below this node's token-rate.
  for (Node* p = parent_; p; p = p->parent_) {
    assert(n <= p->actualRate());
    assert(p->actualChildRate() <= p->actualRate() - n);

    p->actualRate_ -= n;
  }
}

template <typename T>
void TokenShaper<T>::Node::enqueue(T* value) {
  queue_.emplace_back(value, MonotonicClock::now());

  ++queued_;

  updateQueueTimer();
}

/**
 * Fairly dequeues an item from this node or from any of the child nodes.
 *
 * By fair, we mean, that the child nodes always take precedence before
 * this node itself.
 * Although, the child nodes are iterated round-robin.
 *
 * An item gets only dequeued from a node if and only if (1.) there is
 * something to be dequeued and (2.) this node has some tokens available.
 *
 * \returns nullptr if nothing could be dequeued or a pointer to the dequeued
 *          value if there is one <b>and</b> the referring node had a token
 *available.
 */
template <typename T>
T* TokenShaper<T>::Node::dequeue() {
  // Do we have child buckets? Then always first dequeue requests from the
  // childs.
  for (size_t i = 0, childCount = children_.size(); i < childCount; ++i) {
    // In order to preserve fairness across all direct child buckets,
    // we keep an index of where we dequeued the last, and try to dequeue
    // the next one relative from there, like RR.
    dequeueOffset_ = dequeueOffset_ == 0 ? childCount - 1 : dequeueOffset_ - 1;

    if (T* token = children_[dequeueOffset_]->dequeue()) {
      return token;
    }
  }

  // We could not actually dequeue request from any of the child buckets,
  // so try in current bucket itself, if its queue is non-empty.
  if (!queue_.empty()) {
    TS_TRACE("dequeue(): try dequeing from $0, queue size $1", name_, queue_.size());
    if (get(1)) {  // XXX token count of 1 is hard coded. doh.
      QueueItem item = queue_.front();
      queue_.pop_front();
      --queued_;
      return item.token;
    }
  }

  // No request have been found to be dequeued.
  return nullptr;
}

template <typename T>
inline const Counter& TokenShaper<T>::Node::queued() const noexcept {
  return queued_;
}

template <typename T>
inline unsigned long long TokenShaper<T>::Node::dropped() const {
  return dropped_.load();
}

template <typename T>
inline Duration TokenShaper<T>::Node::queueTimeout() const {
  return queueTimeout_;
}

template <typename T>
inline void TokenShaper<T>::Node::setQueueTimeout(Duration value) {
  queueTimeout_ = value;
  for (auto child: children_) {
    child->setQueueTimeout(value);
  }
}

template <typename T>
inline bool TokenShaper<T>::Node::empty() const {
  return children_.empty();
}

template <typename T>
inline size_t TokenShaper<T>::Node::size() const {
  return children_.size();
}

template <typename T>
inline typename TokenShaper<T>::Node::BucketList::const_iterator
TokenShaper<T>::Node::begin() const {
  return children_.begin();
}

template <typename T>
inline typename TokenShaper<T>::Node::BucketList::const_iterator
TokenShaper<T>::Node::end() const {
  return children_.end();
}

template <typename T>
inline typename TokenShaper<T>::Node::BucketList::const_iterator
TokenShaper<T>::Node::cbegin() const {
  return children_.cbegin();
}

template <typename T>
inline typename TokenShaper<T>::Node::BucketList::const_iterator
TokenShaper<T>::Node::cend() const {
  return children_.cend();
}

template <typename T>
void TokenShaper<T>::Node::writeJSON(JsonWriter& json) const {
  json.beginObject()
      .name("name")(name())
      .name("rate")(ratePercent_)
      .name("ceil")(ceilPercent_)
      .name("token-rate")(rate())
      .name("token-ceil")(ceil())
      .beginObject("stats")
      .name("load")(actualRate_)
      .name("queued")(queued())
      .name("dropped")(dropped())
      .endObject();

  json.beginArray("children");
  for (auto n : children_) {
    n->writeJSON(json);
  }
  json.endArray();

  json.endObject();
}

template <typename T>
void TokenShaper<T>::Node::updateQueueTimer() {
  MonotonicTime now = MonotonicClock::now();

  // finish already timed out requests
  while (!queue_.empty()) {
    QueueItem front = queue_.front();
    Duration age = now - front.ctime;
    if (age < queueTimeout_)
      break;

    TS_TRACE("updateQueueTimer: dequeueing timed out request. $0 < $1",
             age, queueTimeout_);
    queue_.pop_front();
    --queued_;
    ++dropped_;

    if (onTimeout_) {
      onTimeout_(front.token);
    }
  }

  if (queue_.empty()) {
    // TRACE("updateQueueTimer: queue empty. not starting new timer.");
    return;
  }

  // setup queue timer to wake up after next timeout is reached.
  QueueItem front = queue_.front();
  Duration age = now - front.ctime;
  Duration ttl = queueTimeout_ - age;

  TS_TRACE("updateQueueTimer: starting new timer with TTL $0, age $1", ttl, age);

  executor_->executeAfter(
      ttl,
      std::bind(&TokenShaper<T>::Node::updateQueueTimer, this));
}

template <typename T>
void TokenShaper<T>::Node::onTimeout() {
  updateQueueTimer();
}
// }}}
// {{{ dumpNode / dump
template <typename T>
void dumpNode(const T* bucket, const char* title, int depth) {
  if (title && *title)
    printf("%10s: ", title);
  else
    printf("%10s  ", "");

  for (int i = 0; i < depth; ++i)
    printf(" -- ");

  printf(
      "name:%-20s rate:%-2zu (%.2f) ceil:%-2zu (%.2f) \tactual-rate:%-2zu "
      "queued:%-2zu\n",
      AnsiColor::colorize(AnsiColor::Green, bucket->name()).c_str(),
      bucket->rate(), bucket->rateP(), bucket->ceil(), bucket->ceilP(),
      bucket->actualRate(), bucket->queued().current());

  for (const auto& child : *bucket) dumpNode(child, "", depth + 1);

  if (!depth) {
    printf("\n");
  }
}

template <class T>
inline void dump(const TokenShaper<T>& shaper, const char* title) {
  dumpNode(shaper.rootNode(), title, 0);
}
// }}}

} // namespace xzero
