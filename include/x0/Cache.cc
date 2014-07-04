// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <vector>

namespace x0 {

template <typename Key, typename Value>
inline Cache<Key, Value>::Cache(std::size_t maxcost)
    : hash_(), first_(0), last_(0), max_cost_(maxcost), cur_cost_(0) {}

template <typename Key, typename Value>
inline Cache<Key, Value>::~Cache() {
  clear();
}

template <typename Key, typename Value>
inline bool Cache<Key, Value>::insert(const Key&& key, Value* value,
                                      std::size_t cost) {
  remove(key);

  if (cost > max_cost_) {
    delete value;
    return false;
  }

  trim(max_cost_ - cost);

  Node sn(value, cost);
  auto i = hash_.insert(std::make_pair(key, sn)).first;
  cur_cost_ += cost;
  Node* n = &i->second;
  n->key_ptr = &i->first;
  if (first_) first_->previous = n;
  n->next = first_;
  first_ = n;
  if (!last_) last_ = first_;

  return true;
}

template <typename Key, typename Value>
inline bool Cache<Key, Value>::contains(const Key&& key) const {
  return hash_.find(key) != hash_.end();
}

template <typename Key, typename Value>
inline typename Cache<Key, Value>::iterator Cache<Key, Value>::find(
    const Key&& key) {
  return hash_.find(key);
}

template <typename Key, typename Value>
inline Value* Cache<Key, Value>::get(const Key&& key) const {
  return const_cast<Cache<Key, Value>*>(this)->relink(key);
}

template <typename Key, typename Value>
inline void Cache<Key, Value>::remove(const Key&& key) {
  erase(hash_.find(key));
}

template <typename Key, typename Value>
inline void Cache<Key, Value>::erase(const iterator&& i) {
  if (i != hash_.end()) unlink(i->second);
}

template <typename Key, typename Value>
inline void Cache<Key, Value>::clear() {
  while (first_) {
    delete first_->value_ptr;
    first_ = first_->next;
  }

  hash_.clear();
  cur_cost_ = 0;
}

template <typename Key, typename Value>
inline Value* Cache<Key, Value>::operator[](const Key&& key) const {
  return get(key);
}

template <typename Key, typename Value>
inline Value* Cache<Key, Value>::operator()(const Key&& key) const {
  return get(key);
}

template <typename Key, typename Value>
inline bool Cache<Key, Value>::operator()(const Key&& key, Value* value,
                                          std::size_t cost) {
  return insert(key, value, cost);
}

template <typename Key, typename Value>
inline std::size_t Cache<Key, Value>::max_cost() const {
  return max_cost_;
}

template <typename Key, typename Value>
inline void Cache<Key, Value>::max_cost(std::size_t value) {
  max_cost_ = value;
  trim(max_cost_);
}

template <typename Key, typename Value>
inline std::size_t Cache<Key, Value>::cost() const {
  return cur_cost_;
}

template <typename Key, typename Value>
inline std::size_t Cache<Key, Value>::size() const {
  return hash_.size();
}

template <typename Key, typename Value>
inline bool Cache<Key, Value>::empty() const {
  return hash_.empty();
}

template <typename Key, typename Value>
inline std::vector<Key> Cache<Key, Value>::keys() const {
  std::vector<Key> result;
  result.reserve(hash_.size());

  for (auto i = hash_.begin(), e = hash_.end(); i != e; ++i)
    result.push_back(i->first);

  return result;
}

template <typename Key, typename Value>
inline void Cache<Key, Value>::trim(std::size_t limit) {
  for (Node* n = last_; n && cur_cost_ > limit;) {
    Node* u = n;
    n = n->previous;
    unlink(*u);
  }
}

template <typename Key, typename Value>
inline void Cache<Key, Value>::unlink(Node& n) {
  if (n.previous) n.previous->next = n.next;
  if (n.next) n.next->previous = n.previous;

  if (&n == last_) last_ = n.previous;
  if (&n == first_) first_ = n.next;

  cur_cost_ -= n.cost;
  delete n.value_ptr;

  hash_.erase(hash_.find(*n.key_ptr));
}

template <typename Key, typename Value>
inline Value* Cache<Key, Value>::relink(const Key&& key) {
  auto i = hash_.find(key);
  if (i == hash_.end()) return 0;

  Node&& n = i->second;

  if (first_ != &n) {
    if (n.previous) n.previous->next = n.next;
    if (n.next) n.next->previous = n.previous;
    if (last_ == &n) last_ = n.previous;
    n.previous = 0;
    n.next = first_;

    first_->previous = &n;
    first_ = &n;
  }

  return n.value_ptr;
}

}  // namespace x0

// vim:syntax=cpp
