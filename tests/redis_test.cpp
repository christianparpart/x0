// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <initializer_list>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <x0/cache/Redis.h>
#include <x0/Buffer.h>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

#ifndef NDEBUG
#define NDEBUG 1
#endif

using namespace x0;

class RedisTest : public CPPUNIT_NS::TestFixture {
 public:
  CPPUNIT_TEST_SUITE(RedisTest);
  CPPUNIT_TEST(parse_number);
  CPPUNIT_TEST(parse_status);
  CPPUNIT_TEST(parse_error);
  CPPUNIT_TEST(parse_string1);
  CPPUNIT_TEST(parse_string2);
  // TODO CPPUNIT_TEST(parse_array1);
  CPPUNIT_TEST_SUITE_END();

 private:
  void parse_number() {
    Buffer msg = ":12345\r\n";
    Redis::MessageParser p(&msg);
    p.parse();

    CPPUNIT_ASSERT(p.state() == Redis::MessageParser::MESSAGE_END);
    CPPUNIT_ASSERT(p.message() != nullptr);
    CPPUNIT_ASSERT(p.message()->type() == Redis::Message::Number);
    CPPUNIT_ASSERT(p.message()->toNumber() == 12345);
  }

  void parse_status() {
    Buffer msg = "+Hello World\r\n";
    Redis::MessageParser p(&msg);
    p.parse();

    CPPUNIT_ASSERT(p.state() == Redis::MessageParser::MESSAGE_END);
    CPPUNIT_ASSERT(p.message() != nullptr);
    CPPUNIT_ASSERT(p.message()->size() == 11);
    CPPUNIT_ASSERT(strcmp(p.message()->toString(), "Hello World") == 0);
    CPPUNIT_ASSERT(p.message()->type() == Redis::Message::Status);
  }

  void parse_error() {
    Buffer msg = "-Hello World\r\n";
    Redis::MessageParser p(&msg);
    p.parse();

    CPPUNIT_ASSERT(p.state() == Redis::MessageParser::MESSAGE_END);
    CPPUNIT_ASSERT(p.message() != nullptr);
    CPPUNIT_ASSERT(p.message()->size() == 11);
    CPPUNIT_ASSERT(strcmp(p.message()->toString(), "Hello World") == 0);
    CPPUNIT_ASSERT(p.message()->type() == Redis::Message::Error);
  }

  void parse_string1() {
    Buffer msg = "$11\r\nHello World\r\n";
    Redis::MessageParser p(&msg);
    p.parse();

    CPPUNIT_ASSERT(p.state() == Redis::MessageParser::MESSAGE_END);
    CPPUNIT_ASSERT(p.message() != nullptr);
    CPPUNIT_ASSERT(p.message()->type() == Redis::Message::String);
    CPPUNIT_ASSERT(p.message()->size() == 11);
    CPPUNIT_ASSERT(strcmp(p.message()->toString(), "Hello World") == 0);
  }

  void parse_string2() {
    Buffer msg = "$12\r\nHello\r\nWorld\r\n";
    Redis::MessageParser p(&msg);
    p.parse();

    CPPUNIT_ASSERT(p.state() == Redis::MessageParser::MESSAGE_END);
    CPPUNIT_ASSERT(p.message() != nullptr);
    CPPUNIT_ASSERT(p.message()->type() == Redis::Message::String);
    CPPUNIT_ASSERT(p.message()->size() == 12);
    CPPUNIT_ASSERT(strcmp(p.message()->toString(), "Hello\r\nWorld") == 0);
  }

  // TODO
  void parse_array1() {
    Buffer msg =
        "*1\r\n"
        "$11\r\n"
        "Hello World\r\n";

    Redis::MessageParser p(&msg);
    p.parse();

    CPPUNIT_ASSERT(p.state() == Redis::MessageParser::MESSAGE_END);
    CPPUNIT_ASSERT(p.message() != nullptr);
    CPPUNIT_ASSERT(p.message()->type() == Redis::Message::Array);
    CPPUNIT_ASSERT(p.message()->size() == 1);
    // CPPUNIT_ASSERT(p.message()->toArray()[0] != 0);
  }

  void parse_array2() {
    Buffer msg =
        "*1\r\n"
        "$11\r\n"
        "Hello World\r\n"
        "$9\r\n"
        "Hi, Redis\r\n";

    Redis::MessageParser p(&msg);
    p.parse();
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(RedisTest);
