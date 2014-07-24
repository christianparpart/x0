// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/Api.h>
#include <base/Defines.h>
#include <base/sysconfig.h>

#if !defined(BASE_QUEUE_LOCKFREE)
#include <mutex>
#include <deque>
#endif

namespace base {

/*! Implements a basic lock free & non-blocking queue.
 *
 * See http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf for
 *further
 * implementation details.
 */
template <typename T>
class BASE_API Queue {
 private:
#if defined(BASE_QUEUE_LOCKFREE)
  struct NodePtr;
  struct Node;

  NodePtr front_;
  NodePtr back_;
#else
  std::mutex lock_;
  std::deque<T> impl_;
#endif

 public:
  Queue();
  ~Queue();

  void enqueue(const T& value);
  void enqueue(T&& value);
  bool dequeue(T* result);

  bool empty() const;
  const T& front() const;
  T& front();

 private:
#if defined(BASE_QUEUE_LOCKFREE)
  void enqueue(Node* value);
#endif
};

#if defined(BASE_QUEUE_LOCKFREE)
// {{{ Queue<T>::NodePtr impl
template <typename T>
struct Queue<T>::NodePtr {
  Node* ptr;
  unsigned count;

  explicit NodePtr(Node* p = nullptr, unsigned c = 0) : ptr(p), count(c) {}

  NodePtr(const NodePtr& np) : ptr(np.ptr), count(np.count) {}

  NodePtr(const NodePtr* np) : ptr(nullptr), count(0) {
    if (likely(np != nullptr)) {
      count = np->count;
      ptr = np->ptr;
    }
  }

  Node* get() const { return ptr; }

  Node* operator->() { return ptr; }

  bool compareAndSwap(const NodePtr& expected, const NodePtr& exchange) {
    if (expected.ptr ==
        __sync_val_compare_and_swap(&ptr, expected.ptr, exchange.ptr)) {
      __sync_lock_test_and_set(&count, exchange.count);
      return true;
    }
    return false;
  }
};
// }}}
// {{{ Queue<T>::Node impl
template <typename T>
struct BASE_API Queue<T>::Node {
  T value;
  NodePtr next;

  Node() : value(), next() {}
  explicit Node(const T& v) : value(v), next() {}
  explicit Node(T&& v) : value(std::move(v)), next() {}
};
// }}}
// {{{ Queue<T> impl
template <typename T>
Queue<T>::Queue()
    : front_(new Node()), back_(front_.ptr) {}

template <typename T>
Queue<T>::~Queue() {
  // delete dummy-node
  // delete marker_;
  delete front_.ptr;
}

template <typename T>
void Queue<T>::enqueue(const T& value) {
  enqueue(new Node(value));
}

template <typename T>
void Queue<T>::enqueue(T&& value) {
  enqueue(new Node(std::move(value)));
}

template <typename T>
void Queue<T>::enqueue(Node* n) {
  while (true) {
    NodePtr back(back_);
    NodePtr next(back.get() ? back->next.get() : nullptr,
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

template <typename T>
inline bool Queue<T>::empty() const {
  // return front_.ptr == marker_;
  return front_.ptr == back_.ptr;
}

template <typename T>
inline const T& Queue<T>::front() const {
  return front_.ptr->value;
}

template <typename T>
inline T& Queue<T>::front() {
  return front_.ptr->value;
}

template <typename T>
bool Queue<T>::dequeue(T* result) {
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
        // read value before CAS otherwise another deque might try to free the
        // next node
        *result = std::move(next.ptr->value);

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
// }}}
#else
// {{{
template <typename T>
Queue<T>::Queue()
    : lock_(), impl_() {}

template <typename T>
Queue<T>::~Queue() {}

template <typename T>
void Queue<T>::enqueue(const T& value) {
  std::lock_guard<std::mutex> _l(lock_);
  impl_.push_back(value);
}

template <typename T>
inline bool Queue<T>::empty() const {
  std::lock_guard<std::mutex> _l(lock_);
  return impl_.empty();
}

template <typename T>
inline const T& Queue<T>::front() const {
  std::lock_guard<std::mutex> _l(lock_);
  return impl_.front();
}

template <typename T>
inline T& Queue<T>::front() {
  std::lock_guard<std::mutex> _l(lock_);
  return impl_.front();
}

template <typename T>
bool Queue<T>::dequeue(T* result) {
  std::lock_guard<std::mutex> _l(lock_);

  if (!impl_.empty()) {
    *result = std::move(impl_.front());
    impl_.pop_front();
    return true;
  } else {
    return false;
  }
}
// }}}
#endif

}  // namespace base
