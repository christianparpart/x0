// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "x0/TernarySearch.hpp"
#include <iostream>
#include <exception>
#include <cassert>

#if (1 == 0)
template <typename T>
std::string pretty(T i) {
  return i ? *i : "(null)";
}
#endif

class ternary_search_test : public CPPUNIT_NS::TestFixture {
 public:
  CPPUNIT_TEST_SUITE(ternary_search_test);
  CPPUNIT_TEST(simple);
  CPPUNIT_TEST(match_direct);
  CPPUNIT_TEST(match_indirect);
  CPPUNIT_TEST(not_found);
  //		CPPUNIT_TEST(iterate1);
  CPPUNIT_TEST_SUITE_END();

 private:
  /** softly validates an iterator value.
   * \param i the iterator to test
   * \param e the `end()`-iterator (which \p i MUST not be)
   * \param c the test-value we expect \p *i to be.
   * \retval true \p *i equals \p c, that is, i is valid.
   * \retval false \p i does not represent the valid and expected value.
   */
  template <typename T, typename U>
  bool test(T&& i, T&& e, U&& c) {
    if (i == e) return false;

    if (*i != c) return false;

    return true;
  }

 private:
  void simple() {
    base::ternary_search<std::string, std::string> m;

    m.insert("/", "some /");
    m.insert("/foo/", "some /foo");
    m.insert("/bar/", "some /bar");
    m.insert("/block/", "some /block");

    CPPUNIT_ASSERT(m.size() == 4);

    CPPUNIT_ASSERT(test(m.find("/"), m.end(), "some /"));
    CPPUNIT_ASSERT(test(m.find("/foo/"), m.end(), "some /foo"));
    CPPUNIT_ASSERT(test(m.find("/foo/bar"), m.end(), "some /foo"));
    CPPUNIT_ASSERT(test(m.find("/bar/"), m.end(), "some /bar"));
    CPPUNIT_ASSERT(test(m.find("/bar/bar"), m.end(), "some /bar"));
    CPPUNIT_ASSERT(test(m.find("/block/"), m.end(), "some /block"));
    CPPUNIT_ASSERT(test(m.find("/blocked"), m.end(), "some /"));
  }

  void match_direct() {
    base::ternary_search<std::string, std::string> m;

    m.insert("/", "some /");
    m.insert("/foo/", "some /foo/");

    CPPUNIT_ASSERT(test(m.find("/"), m.end(), "some /"));
    CPPUNIT_ASSERT(test(m.find("/foo/"), m.end(), "some /foo/"));
  }

  void match_indirect() {
    base::ternary_search<std::string, std::string> m;

    m.insert("/foo/", "some /foo/");
    m.insert("/foo/bar/", "some /foo/bar/");

    CPPUNIT_ASSERT(test(m.find("/foo/"), m.end(), "some /foo/"));
    CPPUNIT_ASSERT(test(m.find("/foo/foo/"), m.end(), "some /foo/"));
    CPPUNIT_ASSERT(test(m.find("/foo/bar"), m.end(), "some /foo/"));
    CPPUNIT_ASSERT(test(m.find("/foo/bar/"), m.end(), "some /foo/bar/"));
  }

  void not_found() {
    base::ternary_search<std::string, std::string> m;

    CPPUNIT_ASSERT(m.find("-bad") == m.end());

    m.insert("-bible", "-bible-value");
    CPPUNIT_ASSERT(m.find("-bad") == m.end());
  }

  void iterate1() {
    base::ternary_search<std::string, std::string> m;

    CPPUNIT_ASSERT(m.begin() == m.end());

    m.insert("/foo/", "some /foo/");
    base::ternary_search<std::string, std::string>::iterator i;

    i = m.begin();
    CPPUNIT_ASSERT(i != m.end());
    CPPUNIT_ASSERT(*i == "some /foo/");

    ++i;
    CPPUNIT_ASSERT(i == m.end());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(ternary_search_test);
