/* <x0/Queue.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/Defines.h>

namespace x0 {

/*! Implements a basic lock free & non-blocking queue.
 *
 * See http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf for further
 * implementation details.
 */
template<typename T>
class X0_API Queue
{
private:
	struct NodePtr;
	struct Node;

	NodePtr front_;
	NodePtr back_;

public:
	Queue();
	~Queue();

	void enqueue(const T& value);
	bool dequeue(T* result);
};

// {{{ Queue<T>::NodePtr impl
template<typename T>
struct Queue<T>::NodePtr
{
	Node* ptr;
	unsigned count;

	NodePtr() : ptr(nullptr), count(0) {}
	NodePtr(Node* p, unsigned c) : ptr(p), count(c) {}

	NodePtr(const NodePtr& np) :
		ptr(np.ptr),
		count(np.count)
	{
	}

	NodePtr(const NodePtr* np) :
		ptr(nullptr),
		count(0)
	{
		if (likely(np != nullptr)) {
			count = np->count;
			ptr = np->ptr;
		}
	}

	Node* get() const {
		return ptr;
	}

	Node* operator->() {
		return ptr;
	}

	bool compareAndSwap(const NodePtr& expected, const NodePtr& exchange) {
		if (expected.ptr == __sync_val_compare_and_swap(&ptr, expected.ptr, exchange.ptr)) {
			__sync_lock_test_and_set(&count, exchange.count);
			return true;
		}
		return false;
	}
};
// }}}
// {{{ Queue<T>::Node impl
template<typename T>
struct Queue<T>::Node
{
	T value;
	NodePtr next;

	Node() : value(), next() {}
	Node(const T& v) : value(v), next() {}
};
// }}}
// {{{ Queue<T> impl
template<typename T>
Queue<T>::Queue() :
	front_(),
	back_()
{
	front_.ptr = back_.ptr = new Node();
}

template<typename T>
Queue<T>::~Queue()
{
	// delete dummy-node
	delete front_.ptr;
}

template<typename T>
void Queue<T>::enqueue(const T& value)
{
	Node* n = new Node(value);

	while (true) {
		NodePtr back(back_);
		NodePtr next(
			back.get() ? back->next.get() : nullptr,
			back.get() ? back->next.count : 0);

		if (back.count == back_.count && back.ptr == back_.ptr) {
			// back pointing to the last node?
			if (next.ptr == nullptr) {
				// try linking node at the end of the list
				if (back->next.compareAndSwap(next, NodePtr(n, next.count + 1))) {
					return;
				}
			} else {
				// Tail was not pointing to the last node,
				// try to swing back to the next node.
				back_.compareAndSwap(back, NodePtr(next.ptr, back.count + 1));
			}
		}
	}
}

template<typename T>
bool Queue<T>::dequeue(T* result)
{
	NodePtr front;

	// keep trying dequeuing until succeed or failed (because empty)
	while (true) {
		front = front_;
		NodePtr back(back_);

		if (front.ptr == nullptr) {
			// Queue empty.
			return false;
		}

		NodePtr next(front->next);

		// Are front, back, and next consistent?
		if (front.count == front_.count && front.ptr == front_.ptr) {
			// Is back falling behind?
			if (front.ptr == back.ptr) {
				// Queue empty?
				if (next.ptr == nullptr) {
					return false;
				}

				// back_ is falling behind. Try to advance it.
				back_.compareAndSwap(back, NodePtr(next.ptr, back.count + 1));
			} else {
				// no need to deal with back
				// read value before CAS otherwise another deque might try to free the next node
				*result = next.ptr->value;

				// try to swing Head to the next node
				if (front_.compareAndSwap(front, NodePtr(next.ptr, front.count + 1))) {
					// done!
					break;
				}
			}
		}
	}

	// It is now safe to free the old dummy node
	delete front.ptr;

	// queue was not empty, deque succeeded
	return true;
}

} // namespace x0
