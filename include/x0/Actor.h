/* <Actor.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_Actor_h
#define sw_x0_Actor_h (1)

#include <x0/Api.h>
#include <x0/Queue.h>

#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>

namespace x0 {

template<typename Message>
class X0_API Actor
{
private:
	bool shutdown_;
	Queue<Message> messages_;
	std::vector<std::future<void>> threads_;

	std::mutex mutex_;
	std::unique_lock<std::mutex> lock_;
	std::condition_variable cond_;

public:
	explicit Actor(size_t scalability = 1);
	virtual ~Actor();

	bool empty() const;
	int scalability() const { return threads_.size(); }

	void send(const Message& message);

	void push_back(const Message& message) { send(message); }
	Actor<Message>& operator<<(const Message& message) { send(message); return *this; }

	void start();
	void stop();
	void join();

protected:
	virtual void process(Message message) = 0;

private:
	void main();
};

// {{{ impl
template<typename Message>
inline Actor<Message>::Actor(size_t scalability) :
	shutdown_(false),
	messages_(),
	threads_(scalability),
	mutex_(),
	lock_(mutex_),
	cond_()
{
}

template<typename Message>
inline Actor<Message>::~Actor()
{
}

template<typename Message>
bool Actor<Message>::empty() const
{
	return messages_.empty();
}

template<typename Message>
inline void Actor<Message>::send(const Message& message)
{
	messages_.enqueue(message);
	cond_.notify_one();
}

template<typename Message>
void Actor<Message>::start()
{
	shutdown_ = false;

	for (auto& thread: threads_) {
		thread = std::move(std::async(std::bind(&Actor<Message>::main, this)));
	}
}

template<typename Message>
inline void Actor<Message>::stop()
{
	shutdown_ = true;
	cond_.notify_all();
}

template<typename Message>
inline void Actor<Message>::join()
{
	for (auto& thread: threads_) {
		thread.wait();
	}
}

template<typename Message>
void Actor<Message>::main()
{
	std::lock_guard<decltype(mutex_)> l(mutex_);

	for (;;) {
		cond_.wait(lock_);

		if (shutdown_)
			break;

		Message message;
		while (messages_.dequeue(&message)) {
			process(message);
		}
	}
}
// }}}

} // namespace x0

#endif
