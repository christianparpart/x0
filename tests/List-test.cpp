// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <gtest/gtest.h>
#include <x0/List.h>
#include <vector>

using namespace x0;

std::vector<int> vectorize(List<int>& r) {
  std::vector<int> v;
  r.each([&](int& value)->bool {
    v.push_back(value);
    return true;
  });
  return v;
}

void dump(List<int>& list) {
  printf("Dumping %zu items\n", list.size());
  int i = 0;
  for (auto& item : list) {
    printf("[%d] %d\n", i, item);
    ++i;
  }
}

TEST(ListTest, Empty) {
  List<int> r;
  ASSERT_TRUE(r.empty());
  ASSERT_FALSE(!r.empty());
  ASSERT_EQ(0, r.size());
}

TEST(ListTest, PushFront) {
  List<int> r;
  r.push_front(7);
  ASSERT_EQ(1, r.size());
  ASSERT_EQ(7, r.front());

  r.push_front(9);
  ASSERT_EQ(2, r.size());
  ASSERT_EQ(9, r.front());
}

TEST(ListTest, PushBack) {
  List<int> r;
  r.push_back(1);
  r.push_back(2);
  r.push_back(3);
  ASSERT_EQ(3, r.size());

  auto v = vectorize(r);
  ASSERT_EQ(1, v[0]);
  ASSERT_EQ(2, v[1]);
  ASSERT_EQ(3, v[2]);
  ASSERT_EQ(3, v.size());
}

TEST(ListTest, PopFront) {
  List<int> r;
  r.push_front(1);
  r.push_front(2);
  r.push_front(3);
  ASSERT_EQ(3, r.pop_front());
  ASSERT_EQ(2, r.pop_front());
  ASSERT_EQ(1, r.pop_front());
}

TEST(ListTest, Remove) {
  List<int> r;
  r.push_back(1);
  r.push_back(2);
  r.push_back(3);

  r.remove(2);
  ASSERT_EQ(2, r.size());

  auto v = vectorize(r);
  ASSERT_EQ(2, v.size());
  ASSERT_EQ(1, v[0]);
  ASSERT_EQ(3, v[1]);
}

TEST(ListTest, Each) {
  List<int> r;
  r.push_front(1);
  r.push_front(2);
  r.push_front(3);

  auto v = vectorize(r);
  ASSERT_EQ(3, v.size());
  ASSERT_EQ(3, v[0]);
  ASSERT_EQ(2, v[1]);
  ASSERT_EQ(1, v[2]);
}
