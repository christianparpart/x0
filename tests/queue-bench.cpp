#include <x0/Queue.h>
#include <deque>
#include <vector>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <unistd.h>
#include <x0/TimeSpan.h>

using namespace x0;

class StopWatch {
private:
	ev_tstamp start_;

public:
	StopWatch() :
		start_()
	{
		reset();
	}

	void reset()
	{
		start_ = now();
	}

	static ev_tstamp now()
	{
		const ev_tstamp NS = 1000000000.0;
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return static_cast<ev_tstamp>(ts.tv_sec) + static_cast<ev_tstamp>(ts.tv_nsec) / NS;
	}

	TimeSpan get() const
	{
		return TimeSpan(now() - start_);
	}

	static TimeSpan resolution()
	{
		timespec now;
		clock_getres(CLOCK_MONOTONIC, &now);

		const ev_tstamp NS = 1000000000.0;
		ev_tstamp ts = static_cast<ev_tstamp>(now.tv_sec) + static_cast<ev_tstamp>(now.tv_nsec) / NS;

		return TimeSpan(ts);
	}
};

template<typename T>
class StdQueue {
public:
	pthread_spinlock_t lock;
	std::deque<T> queue;

public:
	StdQueue() {
		pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE);
	}

	~StdQueue() {
		pthread_spin_destroy(&lock);
	}

	void enqueue(T value) {
		pthread_spin_lock(&lock);
		queue.push_back(value);
		pthread_spin_unlock(&lock);
	}

	bool dequeue(T* result) {
		pthread_spin_lock(&lock);

		bool success = true;
		if (!queue.empty()) {
			*result = queue.front();
			queue.pop_front();
		} else {
			success = false;
		}

		pthread_spin_unlock(&lock);

		return success;
	}
};

template<template<typename> class QueueT>
class QueueTest
{
private:
	const char* name_;
	QueueT<int> queue_;
	bool done_;

public:
	explicit QueueTest(const char* name) :
		name_(name),
		queue_(),
		done_(false)
	{
	}

	~QueueTest()
	{
	}

	clock_t operator()()
	{
		clock_t a = clock();
		StopWatch sw;

		std::vector<pthread_t> consumer(1); // we can't go beyond 1 consumer for this implementation of Queue<>
		for (auto& c: consumer)
			pthread_create(&c, nullptr, &consume, this);

		pthread_t producer;
		pthread_create(&producer, nullptr, &produce, this);

		void* wayne = nullptr;
		pthread_join(producer, &wayne);

		for (auto& c: consumer)
			pthread_join(c, &wayne);

		clock_t b = clock();
		TimeSpan b2 = sw.get();

		printf("%s: time: %zi\n", name_, b - a);
		printf("%s: time: %s\n", name_, b2.str().c_str());

		return b - a;
	}

	static void* produce(void* p)
	{
		auto* self = (QueueTest<QueueT>*) p;
		printf("%s: producing ...\n", self->name_);

		size_t count = 100000000;
		for (size_t i = 0; i < count; ++i) {
			self->queue_.enqueue(i);
		}

		printf("%s: enqueued %zu items\n", self->name_, count);
		self->done_ = true;

		return nullptr;
	}

	static void* consume(void* p)
	{
		auto* self = (QueueTest<QueueT>*) p;
		printf("%s: consuming ...\n", self->name_);

		size_t i = 0;
		size_t rounds = 0;

		while (!self->done_) {
			int result = 0;
			while (self->queue_.dequeue(&result)) {
				++i;
			}
			++rounds;
		}

		printf("%s: dequeued %zu items in %zu rounds.\n", self->name_, i, rounds);

		return nullptr;
	}
};

int main()
{
	clock_t a = QueueTest<StdQueue>("StdQueue<>")();
	clock_t b = QueueTest<Queue>("Queue<>")();

	printf("factor: %.2f\n", b / float(a));

	return 0;
}
