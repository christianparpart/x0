/* <x0/List.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/TaggedPtr.h>
#include <cstdint>
#include <atomic>

namespace x0 {

template<typename T>
class List {
public:
	class iterator;

	List();
	~List();

	void push_front(const T& value);
	bool remove(const T& value);
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
};

// {{{ struct List<T>::Node
template<typename T>
struct List<T>::Node
{
	Node() : value(), next() {}
	explicit Node(const T& v) : value(v), next() {}

	T value;
	TaggedPtr<Node> next;

	friend class List;
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
	head_(new Node()),
	tail_(new Node()),
	size_(0)
{
	head_->next = tail_;
}

template<typename T>
List<T>::~List()
{
	delete head_.ptr();
	delete tail_.ptr();
}

template<typename T>
void List<T>::push_front(const T& value)
{
	TaggedPtr<Node> n(new Node(value));
	for (;;) {
		TaggedPtr<Node> succ = head_->next;
		n->next = succ;

		if (head_->next.compareAndSwap(succ, n)) {
			++size_;
			break;
		}
	}
}

template<typename T>
bool List<T>::remove(const T& value)
{
	auto pred = head_;

	for (;;) {
		auto curr = pred->next;
		if (curr == tail_)
			return false;

		if (unlikely(curr->value == value)) {
			if (pred->next.compareAndSwap(curr, curr->next)) {
				--size_;
				delete curr.ptr();
				return true;
			}
		} else {
			pred = curr;
			curr = curr->next;
		}
	}
	return false;
}

template<typename T>
T& List<T>::front()
{
	return head_->next->value;
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
bool List<T>::each(const Callback& cb) {
	for (auto& item: *this)
		if (!cb(item))
			return false;
	return true;
}
// }}}

} // namespace x0
