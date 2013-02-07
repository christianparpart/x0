#include <x0/Queue.h>
#include <deque>
#include <vector>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <unistd.h>

using namespace x0;

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

		pthread_t producer;
		pthread_create(&producer, nullptr, &produce, this);

		std::vector<pthread_t> consumer(1); // we can't go beyond 1 consumer for this implementation of Queue<>
		for (auto& c: consumer)
			pthread_create(&c, nullptr, &consume, this);

		void* wayne = nullptr;
		pthread_join(producer, &wayne);

		for (auto& c: consumer)
			pthread_join(c, &wayne);

		clock_t b = clock();

		printf("%s: time: %zi\n", name_, b - a);
		return b - a;
	}

	static void* produce(void* p)
	{
		auto* self = (QueueTest<QueueT>*) p;

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
