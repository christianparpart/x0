#pragma once
/* <x0/TokenShaper.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/TimeSpan.h>
#include <x0/Counter.h>
#include <x0/JsonWriter.h>
#include <ev++.h>
#include <functional>
#include <algorithm>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <cstdint>
#include <cassert>

namespace x0 {

/*! TokenShaper mutation result codes.
 */
enum class TokenShaperError
{
	Success,           //!< Operation has completed successfully.
	RateLimitOverflow, //!< Operation failed as rate limit is either too low or too high.
	CeilLimitOverflow, //!< Operation failed as ceil limit is either too low or too high.
	NameConflict,      //!< Operation failed as given name already exists somewhere else in the tree.
	InvalidChildNode,  //!< Operation failed as this node must not be the root node for the operation to complete.
};

/*! Queuing bucket, inspired by the HTB algorithm, that's being used in Linux traffic shaping.
 *
 * Features:
 * <ul>
 *   <li> Hierarichal Token-based Asynchronous Scheduling </li>
 *   <li> Node-Level Queuing and fair round-robin inter-node dequeuing.</li>
 *   <li> Queue Timeout Management </li>
 * </ul>
 *
 * Since this shaper just ensures whether or not to directly run the task with the given token
 * or not, and if not, it also enqueues it, you have to actually *run* your task that's 
 * being associated with the given token cost.
 *
 * When you have successfully allocated the token(s) for your task with get(),
 * you have to explicitely free them up when your task with put() has finished.
 *
 * \note Not threadsafe.
 */
template<typename T>
class TokenShaper
{
public:
	class Node;

	TokenShaper(ev::loop_ref loop, size_t size);
	~TokenShaper();

	template<typename H>
	void setTimeoutHandler(H handler);

	ev::loop_ref loop() const { return root_->loop_; }

	size_t size() const;
	void resize(size_t capacity);

	Node* rootNode() const;
	Node* findNode(const std::string& name) const;
	TokenShaperError createNode(const std::string& name, float rate, float ceil = 0);
	void destroyNode(Node* n);

	size_t get(size_t tokens = 1) { return root_->get(tokens); }
	void put(size_t tokens = 1) { return root_->put(tokens); }
	T* dequeue() { return root_->dequeue(); }

	void writeJSON(x0::JsonWriter& json) const;

private:
	Node* root_;
};

// {{{ TokenShaper<T>::Node API
template<typename T>
class TokenShaper<T>::Node
{
public:
	typedef std::function<void(T*)> Callback;
	typedef std::vector<TokenShaper<T>::Node*> BucketList;

	~Node();

	Node* parentNode() const { return parent_; }

	const std::string& name() const { return name_; }
	float rate() const { return rate_; }
	float ceil() const { return ceil_; }

	size_t tokenRate() const { return tokenRate_; }
	size_t tokenCeil() const { return tokenCeil_; }

	void setTimeoutHandler(Callback handler);
	TokenShaperError setName(const std::string& value);
	TokenShaperError setRate(float value);
	TokenShaperError setCeil(float value);
	TokenShaperError setRateAndCeil(float rate, float ceil);

	size_t actualTokenRate() const { return load_.current(); }
	size_t tokenOverRate() const { return std::max(static_cast<ssize_t>(actualTokenRate() - tokenRate()), static_cast<ssize_t>(0)); }

	float childRate() const;
	size_t childTokenRate() const;
	size_t actualTokenChildRate() const;
	size_t tokensAvailable() const;

	static TokenShaper<T>::Node* createRoot(ev::loop_ref loop, size_t tokens);
	TokenShaperError createChild(const std::string& name, float rate, float ceil = 0);
	TokenShaper<T>::Node* findChild(const std::string& name) const;
	TokenShaper<T>::Node* rootNode();
	void destroyChild(Node* n);

	TokenShaper<T>::Node& operator[](const std::string& name) const { return *findChild(name); }

	bool send(T* packet, size_t cost = 1);
	size_t get(size_t tokens = 1);
	void put(size_t tokens = 1);

	void enqueue(T* value);
	T* dequeue();

	const x0::Counter& load() const { return load_; }
	const x0::Counter& queued() const { return queued_; }
	const unsigned long long dropped() const { return dropped_.load(); }

	x0::TimeSpan queueTimeout() const { return queueTimeout_; }
	void setQueueTimeout(x0::TimeSpan value) { queueTimeout_ = value; }

	// child bucket access
	bool empty() const { return children_.empty(); }
	size_t size() const { return children_.size(); }
	typename BucketList::const_iterator begin() const { return children_.begin(); }
	typename BucketList::const_iterator end() const { return children_.end(); }
	typename BucketList::const_iterator cbegin() const { return children_.cbegin(); }
	typename BucketList::const_iterator cend() const { return children_.cend(); }

	void writeJSON(x0::JsonWriter& json) const;

private:
	friend class TokenShaper;
	void update(size_t n);
	void update();
	void updateQueueTimer();
	void onTimeout(ev::timer& timer, int revents);
	size_t getReserved(size_t tokens);

private:
	Node(ev::loop_ref loop, const std::string& name, size_t tokenRate, size_t tokenCeil, float rate, float ceil, TokenShaper<T>::Node* parent);

	struct QueueItem {
		T* token;
		ev::tstamp ctime;

		QueueItem() {}
		QueueItem(const QueueItem&) = default;
		QueueItem(T* _token, ev::tstamp _ctime) : token(_token), ctime(_ctime) {}
	};

	ev::loop_ref loop_;

	std::string name_;				//!< bucket name

	size_t tokenRate_;				//!< maximum tokens this bucket and all its children are garanteed.
	size_t tokenCeil_;				//!< maximum tokens this bucket can send if parent has enough tokens spare.

	float rate_;					//!< rate in percent relative to parent's ceil
	float ceil_;					//!< ceil in percent relative to parent's ceil

	Node* parent_;                  //!< parent bucket this bucket is a direct child of
	BucketList children_;           //!< direct child buckets

	x0::Counter load_;              //!< bucket load stats
	x0::Counter queued_;            //!< bucket queue stats
	std::atomic<unsigned long long> dropped_;

	x0::TimeSpan queueTimeout_;     //!< time span on how long a token may stay in queue.
	std::deque<QueueItem> queue_;   //!< FIFO queue of tokens that could not be passed directly.
	ev::timer queueTimer_;          //!< The queue-timeout timer.
	size_t dequeueOffset_;          //!< dequeue-offset at which child to dequeue next.

	Callback onTimeout_;            //!< Callback, invoked when the token has been queued and just timed out.
};
// }}}
// {{{ TokenShaper<T> impl
template<typename T>
TokenShaper<T>::TokenShaper(ev::loop_ref loop, size_t size) :
	root_(Node::createRoot(loop, size))
{
}

template<typename T>
TokenShaper<T>::~TokenShaper()
{
	delete root_;
}

template<typename T>
template<typename H>
void TokenShaper<T>::setTimeoutHandler(H handler)
{
	root_->setTimeoutHandler(handler);
}

template<typename T>
size_t TokenShaper<T>::size() const
{
	return root_->tokenRate();
}

template<typename T>
void TokenShaper<T>::resize(size_t capacity)
{
	// Only recompute tokenRates on child nodes when root node's token rate actually changed.
	if (root_->tokenRate() == capacity)
		return;

	root_->update(capacity);
}

template<typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::rootNode() const
{
	return root_;
}

template<typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::findNode(const std::string& name) const
{
	return root_->findChild(name);
}

template<typename T>
TokenShaperError TokenShaper<T>::createNode(const std::string& name, float rate, float ceil)
{
	return root_->createChild(name, rate, ceil);
}

template<typename T>
void TokenShaper<T>::destroyNode(Node* n)
{
	if (n == root_)
		return;

	n->parentNode()->destroyChild(n);
}

template<typename T>
void TokenShaper<T>::writeJSON(x0::JsonWriter& json) const
{
	return root_->writeJSON(json);
}
// }}}
// {{{ TokenShaper<T>::Node impl
template<typename T>
TokenShaper<T>::Node::Node(ev::loop_ref loop, const std::string& name, size_t tokenRate, size_t tokenCeil, float rate, float ceil, TokenShaper<T>::Node* parent) :
	loop_(loop),
	name_(name),
	tokenRate_(tokenRate),
	tokenCeil_(tokenCeil),
	rate_(rate),
	ceil_(ceil),
	parent_(parent),
	children_(),
	load_(),
	queued_(),
	dropped_(0),
	queueTimeout_(x0::TimeSpan::fromSeconds(10)),
	queue_(),
	queueTimer_(loop),
	dequeueOffset_(0),
	onTimeout_()
{
	if (parent_)
		onTimeout_ = parent_->onTimeout_;

	queueTimer_.set<Node, &Node::onTimeout>(this);
}

template<typename T>
TokenShaper<T>::Node::~Node()
{
}

template<typename T>
float TokenShaper<T>::Node::childRate() const
{
	float sum = 0;

	for (const auto child: children_)
		sum += child->rate();

	return sum;
}

/**
 * Number of tokens reserved by child nodes.
 *
 * This value will be less or equal to this node's computed token-rate.
 */
template<typename T>
size_t TokenShaper<T>::Node::childTokenRate() const
{
	size_t sum = 0;

	for (const auto& child: children_)
		sum += child->tokenRate();

	return sum;
}

template<typename T>
size_t TokenShaper<T>::Node::actualTokenChildRate() const
{
	size_t sum = 0;

	for (const auto& child: children_)
		sum += child->actualTokenRate();

	return sum;
}

template<typename T>
size_t TokenShaper<T>::Node::tokensAvailable() const
{
	return tokenCeil() - actualTokenRate();
}

template<typename T>
void TokenShaper<T>::Node::setTimeoutHandler(Callback handler)
{
	onTimeout_ = handler;
}

template<typename T>
TokenShaperError TokenShaper<T>::Node::setName(const std::string& value)
{
	if (rootNode()->findChild(value))
		return TokenShaperError::NameConflict;

	name_ = value;
	return TokenShaperError::Success;
}

template<typename T>
TokenShaperError TokenShaper<T>::Node::setRate(float newRate)
{
	if (!parent_)
		return TokenShaperError::InvalidChildNode;

	if (newRate < 0.0 || newRate > ceil_)
		return TokenShaperError::RateLimitOverflow;

	rate_ = newRate;
	tokenRate_ = parent_->tokenRate() * rate_;

	for (auto child: children_) {
		child->update();
	}

	return TokenShaperError::Success;
}

template<typename T>
TokenShaperError TokenShaper<T>::Node::setCeil(float newCeil)
{
	if (!parent_)
		return TokenShaperError::InvalidChildNode;

	if (newCeil < rate_ || newCeil > 1.0)
		return TokenShaperError::CeilLimitOverflow;

	ceil_ = newCeil;
	tokenCeil_ = parent_->tokenCeil() * ceil_;

	for (auto child: children_) {
		child->update();
	}

	return TokenShaperError::Success;
}

template<typename T>
TokenShaperError TokenShaper<T>::Node::setRateAndCeil(float newRate, float newCeil)
{
	if (!parent_)
		return TokenShaperError::InvalidChildNode;

	if (newRate < 0.0 || newRate > newCeil)
		return TokenShaperError::RateLimitOverflow;

	if (newCeil > 1.0)
		return TokenShaperError::CeilLimitOverflow;

	rate_ = newRate;
	ceil_ = newCeil;

	update();

	return TokenShaperError::Success;
}

template<typename T>
void TokenShaper<T>::Node::update(size_t capacity)
{
	tokenRate_ = capacity * rate_;
	tokenCeil_ = capacity * ceil_;

	for (auto child: children_) {
		update();
	}
}

template<typename T>
void TokenShaper<T>::Node::update()
{
	if (parent_) {
		tokenRate_ = parent_->tokenRate() * rate_;
		tokenCeil_ = parent_->tokenCeil() * ceil_;
	}

	for (auto child: children_) {
		child->update();
	}
}

template<typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::Node::createRoot(ev::loop_ref loop, size_t tokens)
{
	return new TokenShaper<T>::Node(loop, "root", tokens, tokens, 1.0f, 1.0f, nullptr);
}

template<typename T>
TokenShaperError TokenShaper<T>::Node::createChild(const std::string& name, float rate, float ceil)
{
	// 0 <= rate <= (1 - childRate)
	if (rate < 0.0 || rate + childRate() > 1.0)
		return TokenShaperError::RateLimitOverflow;

	// rate <= ceil <= 1.0
	if (ceil < rate || ceil > 1.0)
		return TokenShaperError::CeilLimitOverflow;

	if (rootNode()->findChild(name))
		return TokenShaperError::NameConflict;

	size_t tokenRate = tokenRate_ * rate;
	size_t tokenCeil = tokenCeil_ * ceil;
	TokenShaper<T>::Node* b = new TokenShaper<T>::Node(loop_, name, tokenRate, tokenCeil, rate, ceil, this);
	children_.push_back(b);
	return TokenShaperError::Success;
}

template<typename T>
void TokenShaper<T>::Node::destroyChild(Node* n)
{
	auto i = std::find(children_.begin(), children_.end(), n);

	if (i != children_.end()) {
		children_.erase(i);
		delete n;
	}
}

template<typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::Node::rootNode()
{
	Node* n = this;

	while (n->parent_ != nullptr)
		n = n->parent_;

	return n;
}

template<typename T>
typename TokenShaper<T>::Node* TokenShaper<T>::Node::findChild(const std::string& name) const
{
	for (auto n: children_)
		if (n->name() == name)
			return n;

	for (auto n: children_)
		if (auto b = n->findChild(name))
			return b;

	return nullptr;
}

/**
 * Tries to allocate \p cost tokens and returns true or enqueus \p packet otherwise and returns false.
 *
 * \retval true requested number of tokens allocated.
 * \retval false no tokens allocated, packet queued on this bucket.
 */
template<typename T>
bool TokenShaper<T>::Node::send(T* packet, size_t cost)
{
	if (get(cost))
		return true;

	enqueue(packet);
	return false;
}

/**
 * Allocates up to \p n tokens from this bucket or nothing if allocation failed.
 *
 * A token is ensured if the actual token rate plus \p n is equal or less than
 * the number of non-reserved tokens.
 *
 * The number of non-reserved tokens equals the node's token rate minus the sum of all children's token rate.
 *
 * If the actual token rate plus \p n is below the node's ceiling, we 
 * attempt to <b>borrow</b> one from the parent.
 *
 * \return the actual number of allocated tokens, which is either \p n (all requested tokens) or \p 0 if failed.
 */
template<typename T>
size_t TokenShaper<T>::Node::get(size_t n)
{
	size_t newRate = actualTokenRate() + n;

	// Attempt to acquire tokens from the ensured token pool.
	if (newRate <= (tokenRate() - childTokenRate())) {
		load_ += n;

		if (parent_) {
			size_t rv = parent_->getReserved(n);
			assert(rv == n);
		}

		return n;
	}

	// Attempt to borrow tokens from parent if and only if the resulting node's rate does not exceed its ceiling.
	if (newRate <= tokenCeil() && parent_ && parent_->get(n)) {
		load_ += n;
		return n;
	}

	// Acquiring %n tokens failed, so return 0 as indication.
	return 0;
}

/**
 * Internal helper function for \c get() that is invoked on parent nodes to also include reserved tokens.
 *
 * \see get(size_t n)
 */
template<typename T>
size_t TokenShaper<T>::Node::getReserved(size_t n)
{
	size_t newRate = actualTokenRate() + n;

	// Attempt to acquire tokens from the ensured token pool.
	if (newRate <= tokenRate()) {
		load_ += n;

		if (parent_) {
			size_t rv = parent_->getReserved(n);
			assert(rv == n);
		}

		return n;
	}

	// Attempt to borrow tokens from parent if and only if the resulting node's rate does not exceed its ceiling.
	if (newRate <= tokenCeil() && parent_ && parent_->get(n)) {
		load_ += n;
		return n;
	}

	// Acquiring n tokens failed, so return 0 as indication.
	return 0;
}

/*!
 * Puts back \p n tokens into the bucket.
 */
template<typename T>
void TokenShaper<T>::Node::put(size_t n)
{
	// you may not refund more tokens than the bucket's ceiling limit.
	assert(n <= actualTokenRate());
	assert(actualTokenChildRate() <= actualTokenRate() - n);

	load_ -= n;

	// Release tokens from parent if the the new actual token rate (load) is still not below this node's token-rate.
	if (parent_) {
		parent_->put(n);
	}
}

template<typename T>
void TokenShaper<T>::Node::enqueue(T* value)
{
	queue_.push_back(QueueItem(value, ev_now(loop_)));

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
 *          value if there is one <b>and</b> the referring node had a token available.
 */
template<typename T>
T* TokenShaper<T>::Node::dequeue()
{
	// Do we have child buckets? Then always first dequeue requests from the childs.
	for (size_t i = 0, childCount = children_.size(); i < childCount; ++i) {
		// In order to preserve fairness across all direct child buckets,
		// we keep an index of where we dequeued the last, and try to dequeue
		// the next one relative from there, like RR.
		dequeueOffset_ = dequeueOffset_ == 0
			? childCount - 1
			: dequeueOffset_ - 1;

		if (T* token = children_[dequeueOffset_]->dequeue()) {
			return token;
		}
	}

	// We could not actually dequeue request from any of the child buckets,
	// so try in current bucket itself, if its queue is non-empty.
	if (!queue_.empty()) {
		if (get()) {
			QueueItem item = queue_.front();
			queue_.pop_front();
			--queued_;
			return item.token;
		}
	}

	// No request have been found to be dequeued.
	return nullptr;
}

template<typename T>
void TokenShaper<T>::Node::writeJSON(x0::JsonWriter& json) const
{
	json.beginObject()
		.name("name")(name())
		.name("rate")(rate())
		.name("ceil")(ceil())
		.name("token-rate")(tokenRate())
		.name("token-ceil")(tokenCeil())
		.name("actual-token-rate")(actualTokenRate())
		.name("load")(load())
		.name("queued")(queued())
		.name("dropped")(dropped());

	json.beginArray("children");
	for (auto n: children_) {
		n->writeJSON(json);
	}
	json.endArray();

	json.endObject();
}

template<typename T>
void TokenShaper<T>::Node::updateQueueTimer()
{
	// quickly return if queue-timer is already running
	if (queueTimer_.is_active())
		return;

	// finish already timed out requests
	while (!queue_.empty()) {
		QueueItem front = queue_.front();
		x0::TimeSpan age(ev_now(loop_) - front.ctime);
		if (age < queueTimeout_)
			break;

		//TRACE("updateQueueTimer: dequeueing timed out request");
		queue_.pop_front();
		--queued_;
		++dropped_;

		if (onTimeout_) {
			onTimeout_(front.token);
		}
	}

	if (queue_.empty()) {
		//TRACE("updateQueueTimer: queue empty. not starting new timer.");
		return;
	}

	// setup queue timer to wake up after next timeout is reached.
	QueueItem front = queue_.front();
	x0::TimeSpan age(ev_now(loop_) - front.ctime);
	x0::TimeSpan ttl(queueTimeout_ - age);
	//TRACE("updateQueueTimer: starting new timer with ttl %.2f secs (%llu ms)", ttl.value(), ttl.totalMilliseconds());
	queueTimer_.start(ttl.value(), 0);
}

template<typename T>
void TokenShaper<T>::Node::onTimeout(ev::timer& timer, int revents)
{
	updateQueueTimer();
}
// }}}

// {{{ JsonWriter
template<typename T>
inline JsonWriter& operator<<(x0::JsonWriter& json, const TokenShaper<T>& value)
{
	value.writeJSON(json);
	return json;
}

template<typename T>
inline JsonWriter& operator<<(x0::JsonWriter& json, const typename TokenShaper<T>::Node& value)
{
	value.writeJSON(json);
	return json;
}
// }}}

} // namespace x0
