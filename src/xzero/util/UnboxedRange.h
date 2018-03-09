// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

namespace xzero {

template<typename T>
class UnboxedRange {
 public:
  using BoxedContainer = T;
  using BoxedIterator = typename BoxedContainer::iterator;
  using element_type = typename BoxedContainer::value_type::element_type;
  class iterator { // {{{
   public:
    explicit iterator(BoxedIterator boxed) : it_(boxed) {}

    const element_type& operator->() const { return **it_; }
    element_type& operator->() { return **it_; }
    element_type operator*() const { return **it_; }

    iterator& operator++() { ++it_; return *this; }
    iterator& operator++(int) { ++it_; return *this; }

    bool operator==(const iterator& other) const { return it_ == other.it_; }
    bool operator!=(const iterator& other) const { return it_ != other.it_; }

   private:
    BoxedIterator it_;
  }; // }}}

  UnboxedRange(BoxedIterator begin, BoxedIterator end) : begin_(begin), end_(end) {}
  explicit UnboxedRange(BoxedContainer& c) : begin_(c.begin()), end_(c.end()) {}

  iterator begin() const { return begin_; }
  iterator end() const { return end_; }

 private:
  iterator begin_;
  iterator end_;
};

/**
 * Unboxes boxed element types in containers.
 *
 * Good examples are:
 *
 * \code
 *    std::vector<std::unique_ptr<int>> numbers;
 *    // ...
 *    for (int number: unbox(numbers)) {
 *      // ... juse use number here, instead of number.get() or *number.
 *    };
 * \endcode
 */
template<typename BoxedContainer>
UnboxedRange<BoxedContainer> unbox(BoxedContainer& boxedContainer) {
  return UnboxedRange<BoxedContainer>(boxedContainer);
}

} // namespace xzero
