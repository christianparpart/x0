// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/Buffer.h>

#include <base/io/Source.h>
#include <base/io/FileSource.h>
#include <base/io/BufferSource.h>
#include <base/io/FilterSource.h>
#include <base/io/CompositeSource.h>

#include <base/io/Sink.h>
#include <base/io/FileSink.h>
#include <base/io/BufferSink.h>

#include <base/io/Filter.h>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>

class safe_pipe {
 private:
  int fd[2];

 public:
  explicit safe_pipe(bool async) {
    fd[0] = fd[1] = -1;

    if (pipe(fd) < 0) perror("pipe2");

    if (async) {
      if (fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK) < 0)
        perror("fcntl");

      if (fcntl(fd[1], F_SETFL, fcntl(fd[1], F_GETFL) | O_NONBLOCK) < 0)
        perror("fcntl");
    }
  }

  int operator[](int i) const {
    assert(i >= 0 && i <= 1);
    assert(fd[i] != -1);
    return fd[i];
  }

  int reader() const { return fd[0]; }
  int writer() const { return fd[1]; }

  ~safe_pipe() {
    for (int i = 0; i < 2; ++i)
      if (fd[i] != -1) ::close(fd[i]);
  }
};

class io_test : public CPPUNIT_NS::TestFixture {
 public:
  CPPUNIT_TEST_SUITE(io_test);
  // source
  CPPUNIT_TEST(test_BufferSource);
  CPPUNIT_TEST(test_file_source);
  CPPUNIT_TEST(test_FilterSource);
  CPPUNIT_TEST(test_CompositeSource);

  // sink
  CPPUNIT_TEST(test_buffer_sink);
  CPPUNIT_TEST(test_file_sink);
  CPPUNIT_TEST_SUITE_END();

 private:
  void test_BufferSource() { /*
                                base::BufferSource s(base::Buffer("hello"));

                                base::Buffer output;
                                base::BufferRef ref = s.pull(output);

                                CPPUNIT_ASSERT(output == "hello");
                                CPPUNIT_ASSERT(ref == output);
                            */
  }

  /* void test_fd_source()
  {
      safe_pipe pfd(true);
      base::SystemSource in(pfd.reader());
      base::Buffer output;

      ::write(pfd.writer(), "12345", 5);
      CPPUNIT_ASSERT(in.pull(output) == "12345");
      CPPUNIT_ASSERT(output == "12345");

      ::write(pfd.writer(), "abcd", 4);
      CPPUNIT_ASSERT(in.pull(output) == "abcd");
      CPPUNIT_ASSERT(output == "12345abcd");

      CPPUNIT_ASSERT(in.pull(output) == "");
      CPPUNIT_ASSERT(errno == EAGAIN);
  } */

  void test_file_source() {
#if 0  // needs porting to new API
        using namespace base;

        fileinfo_ptr info(new fileinfo(__FILE__));
        file_ptr infile(new file(info, O_RDONLY));
        CPPUNIT_ASSERT(infile->handle() != -1);

        file_source in(infile);
        buffer_sink out;

        while (in.pump(out)) ;

        CPPUNIT_ASSERT(!out.buffer().empty());
#endif
  }

  void test_FilterSource() {
    //! \todo
  }

  void test_CompositeSource() {
    /*using namespace base;

    CompositeSource s;
    s.push_back<BufferSource>(ConstBuffer("hello"));
    s.push_back<BufferSource>(Buffer(", "));
    s.push_back<BufferSource>(Buffer("world"));

    Buffer output;
    CPPUNIT_ASSERT(s.pull(output) == "hello");
    CPPUNIT_ASSERT(s.pull(output) == ", ");
    CPPUNIT_ASSERT(s.pull(output) == "world");
    CPPUNIT_ASSERT(!s.pull(output));
    CPPUNIT_ASSERT(output == "hello, world");*/
  }

  void test_buffer_sink() { /*
                               using namespace base;

                               BufferSource src(ConstBuffer("Hello World!"));
                               BufferSink snk;
                               src.pump(snk);

                               CPPUNIT_ASSERT(snk.buffer() == "Hello World!");
                           */
  }

  void test_file_sink() {
    //! \todo
  }

 private:
 private:  // {{{ debug helper
  void print(const base::Buffer& b, const char* msg = 0) {
    if (msg && *msg)
      printf("\nbuffer(%s): '%s'\n", msg, b.str().c_str());
    else
      printf("\nbuffer: '%s'\n", b.str().c_str());
  }

  void print(const base::BufferRef& v, const char* msg = 0) {
    char prefix[64];
    if (msg && *msg)
      snprintf(prefix, sizeof(prefix), "\nbuffer.view(%s)", msg);
    else
      snprintf(prefix, sizeof(prefix), "\nbuffer.view");

    if (v)
      printf("\n%s: '%s' (size=%ld)\n", prefix, v.str().c_str(), v.size());
    else
      printf("\n%s: NULL\n", prefix);
  }
  //}}}
};

CPPUNIT_TEST_SUITE_REGISTRATION(io_test);
