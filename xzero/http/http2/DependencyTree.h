// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <vector>

namespace xzero {

template<typename Item, typename Compare>
class DependencyTree {
 public:
  DependencyTree();
  ~DependencyTree();

  /**
   * Tests whether there are any items in this tree.
   */
  bool empty() const noexcept;

  /**
   * Puts given @p value as a child below @p parent.
   */
  void push_inclusive(Item parent, Item value);

  /**
   * Puts given @p value as a child below @p parent and reparents
   * any already existing child of @p parent below @p value.
   */
  void push_exclusive(Item parent, Item value);

  /**
   * Retrieves the next value that has no dependencies and removes it
   * from the tree.
   */
  bool pop(Value* output);

  /**
   * Retrieves the next value that has no dependencies without popping it off.
   */
  bool peek(Value* output);
 
  struct Node {
    Value value;
    Node* parent;
    std::vector<Node*> children;
    size_t next; //!< index of child to pop next from

    Node(Value _value, Node* _parent)
        : value(_value), parent(_parent), children(), next() {}
    Node(const Node&) = default;
    Node& operator=(const Node&) = default;

    bool pop(Value* output);
    bool peek(Value* output);
  };

 private:
  Node root_;
};

} // namespace xzero
