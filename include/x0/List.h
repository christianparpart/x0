/* <x0/List.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/TaggedPtr.h>
#include <stdint.h>
#include <atomic>

namespace x0 {

template<typename T>
class List {
public:
	class iterator;

	List();
	~List();

	void push_front(const T& value);
	void push_back(const T& value);
	T pop_front();
	bool remove(const T& value);
	void clear();
	size_t size() const;
	bool empty() const;

	T& front();

	iterator begin();
	iterator end();

	template<typename Callback>
	bool each(const Callback& cb);

private:
	struct Node;
	TaggedPtr<Node> head_;		//!< sentinel node for list's head
	TaggedPtr<Node> tail_;		//!< sentinel node for list's tail
	std::atomic<size_t> size_;	//!< actual size of the list

	struct Window;
	Window find(const T& value);

#if !defined(NDEBUG)
	void dump(const TaggedPtr<Node>& node, const char* label);
#endif
};

// {{{ struct List<T>::Node
template<typename T>
struct List<T>::Node
{
	Node() : value(), next(nullptr, 1) {}
	explicit Node(const T& v) : value(v), next(nullptr, 1) {}

	T value;
	TaggedPtr<Node> next;
};
// }}}
// {{{ struct List<T>::Window
template<typename T>
struct List<T>::Window
{
	TaggedPtr<Node> pred;
	TaggedPtr<Node> curr;

	Window(const TaggedPtr<Node>& _pred, const TaggedPtr<Node>& _curr) :
		pred(_pred), curr(_curr) {}
};
// }}}
// {{{ class List<T>::iterator
template<typename T>
class List<T>::iterator
{
private:
	TaggedPtr<Node> current_;

public:
	iterator(const TaggedPtr<Node>& n) : current_(n) {}

	T& get() { return current_->get(); }
	T* operator->() const { return current_.get().value; }
	T& operator*() { return current_.get()->value; }

	bool operator==(const iterator& other) const { return current_ == other.current_; }
	bool operator!=(const iterator& other) const { return current_ != other.current_; }

	iterator& operator++() {
		if (current_->next.get()) {
			current_ = current_->next;
		}
		return *this;
	}
};
// }}}
// {{{ List impl
template<typename T>
List<T>::List() :
	head_(new Node(), 1),
	tail_(new Node(), 1),
	size_(0)
{
	head_->next = tail_;
}

template<typename T>
List<T>::~List()
{
	auto curr = head_;

	while (curr.ptr() != nullptr) {
		auto next = curr->next;
		delete curr.ptr();
		curr = next;
	}
}

//          ------------------
// +--------+   +--------+   +--------+   +--------+   +--------+
// | HEAD   |   | n1     |   | n2     |   | n3     |   | TAIL   |
// +--------+   +--------+   +--------+   +--------+   +--------+
//          -----

template<typename T>
void List<T>::push_front(const T& value)
{
	while (true) {
		// FIXME: head_->next could still be marked as logically deleted
		auto pred = head_;
		auto curr = pred->next;

		TaggedPtr<Node> node(new Node(value), 1);
		node->next = curr;
		if (pred->next.compareAndSwap(curr, node)) {
			++size_;
			break;
		}
	}
}

template<typename T>
void List<T>::push_back(const T& value)
{
	while (true) {
		auto window = find(value);
		auto pred = window.pred;
		auto curr = window.curr;

		TaggedPtr<Node> node(new Node(value), 1);
		node->next = curr;
		if (pred->next.compareAndSwap(curr, node)) {
			++size_;
			break;
		}
	}
}

template<typename T>
T List<T>::pop_front()
{
	T result;
	for (;;) {
		auto pred = head_;
		auto curr = pred->next;

		if (pred->next.compareAndSwap(curr, curr->next)) {
			--size_;
			result = curr->value;
			delete curr.ptr();
			break;
		}
	}
	return result;
}

template<typename T>
bool List<T>::remove(const T& value)
{
	while (true) {
		auto window = find(value);
		auto pred = window.pred;
		auto curr = window.curr;

		if (unlikely(curr->value != value))
			return false;

		auto succ = curr->next;

		//! attempt to mark \c curr as deleted by decrementing \c curr.next's tag by one.
		if (!curr->next.tryTag(succ, succ.tag() - 1))
			continue;

		if (succ.tag() - 1 == 0)
			delete curr.get();

		pred->next.compareAndSwap(curr, succ);
		--size_;
		return true;
	}
}

template<typename T>
void List<T>::clear()
{
	// TODO clear full list by iteratively removing from list head
}

template<typename T>
T& List<T>::front()
{
	while (true) {
		retry:
		auto pred = head_;
		auto curr = pred->next;
		auto succ = curr->next;

		while (succ.tag() == 0) {
			if (pred->next.compareAndSwap(curr, TaggedPtr<Node>(succ.ptr(), succ.tag())))
				goto retry;

			curr = succ;
			succ = curr->next;
		}

		if (curr != tail_)
			return curr->value;

		abort();
	}
}

template<typename T>
size_t List<T>::size() const
{
	return size_;
}

template<typename T>
bool List<T>::empty() const
{
	return size_ == 0;
}

template<typename T>
typename List<T>::iterator List<T>::begin()
{
	return iterator(head_->next);
}

template<typename T>
typename List<T>::iterator List<T>::end()
{
	return iterator(tail_);
}

template<typename T>
template<typename Callback>
bool List<T>::each(const Callback& cb)
{
	for (auto& item: *this)
		if (!cb(item))
			return false;
	return true;
}

#if !defined(NDEBUG)
template<typename T>
void List<T>::dump(const TaggedPtr<Node>& node, const char* label)
{
	if (node == head_)
		printf("%s: HEAD\n", label);
	else if (node == tail_)
		printf("%s: TAIL\n", label);
	else if (node.ptr() == nullptr)
		printf("%s: NULL (%d)\n", label, node.tag());
	else
		printf("%s: %d (%d)\n", label, node->value, node.tag());
}
#endif

template<typename T>
typename List<T>::Window List<T>::find(const T& value)
{
	while (true) {
		retry:
		auto pred = head_;
		auto curr = pred->next;

		while (true) {
			auto succ = curr->next;
			while (succ.tag() == 0) {
				if (!pred->next.compareAndSwap(curr, succ))
					goto retry;

				curr = succ;
				succ = curr->next;
			}

			if (unlikely(curr == tail_ || curr->value == value))
				return Window(pred, curr);

			pred = curr;
			curr = succ;
		}
	}
}
// }}}

} // namespace x0
