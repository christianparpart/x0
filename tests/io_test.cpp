#include <x0/buffer.hpp>

#include <x0/source.hpp>
#include <x0/fd_source.hpp>
#include <x0/file_source.hpp>
#include <x0/buffer_source.hpp>
#include <x0/filter_source.hpp>
#include <x0/composite_source.hpp>

#include <x0/sink.hpp>
#include <x0/fd_sink.hpp>
#include <x0/file_sink.hpp>
#include <x0/buffer_sink.hpp>

#include <x0/filter.hpp>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>
#include <unistd.h>

class safe_pipe
{
private:
	int fd[2];

public:
	explicit safe_pipe(bool async) {
		if (pipe2(fd, async ? O_NONBLOCK : 0) == -1) {
			perror("pipe2");
		}
	}

	int operator[](int i) const {
		assert(i >= 0 && i <= 1);
		assert(fd[i] != -1);
		return fd[i];
	}

	int left() const { return fd[0]; }
	int right() const { return fd[1]; }

	~safe_pipe() {
		for (int i = 0; i < 2; ++i)
			if (fd[i] != -1)
				::close(fd[i]);
	}
};

class io_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(io_test);
		// source
		CPPUNIT_TEST(test_buffer_source);
		CPPUNIT_TEST(test_fd_source);
		CPPUNIT_TEST(test_file_source);
		CPPUNIT_TEST(test_filter_source);
		CPPUNIT_TEST(test_composite_source);

		// sink
		CPPUNIT_TEST(test_buffer_sink);
		CPPUNIT_TEST(test_fd_sink);
		CPPUNIT_TEST(test_file_sink);
	CPPUNIT_TEST_SUITE_END();

private:
	void test_buffer_source()
	{
		x0::buffer_source s(x0::buffer("hello"));

		x0::buffer output;
		x0::buffer_ref ref = s.pull(output);

		CPPUNIT_ASSERT(output == "hello");
		CPPUNIT_ASSERT(ref == output);
	}

	void test_fd_source()
	{
		safe_pipe pfd(true);
		x0::fd_source in(pfd.right());
		x0::buffer output;

		::write(pfd.left(), "12345", 5);
		CPPUNIT_ASSERT(in.pull(output) == "12345");
		CPPUNIT_ASSERT(output == "12345");

		::write(pfd.left(), "abcd", 4);
		CPPUNIT_ASSERT(in.pull(output) == "abcd");
		CPPUNIT_ASSERT(output == "12345abcd");

		CPPUNIT_ASSERT(in.pull(output) == "");
		CPPUNIT_ASSERT(errno == EAGAIN);
	}

	void test_file_source()
	{
		using namespace x0;

		fileinfo_ptr info(new fileinfo(__FILE__));
		file_ptr infile(new file(info, O_RDONLY));
		CPPUNIT_ASSERT(infile->handle() != -1);

		file_source in(infile);
		buffer_sink out;

		while (in.pump(out)) ;

		CPPUNIT_ASSERT(!out.buffer().empty());
	}

	void test_filter_source()
	{
		//! \todo
	}

	void test_composite_source()
	{
		using namespace x0;

		composite_source s;
		s.push_back(source_ptr(new buffer_source(const_buffer("hello"))));
		s.push_back(source_ptr(new buffer_source(buffer(", "))));
		s.push_back(source_ptr(new buffer_source(buffer("world"))));

		buffer output;
		CPPUNIT_ASSERT(s.pull(output) == "hello");
		CPPUNIT_ASSERT(s.pull(output) == ", ");
		CPPUNIT_ASSERT(s.pull(output) == "world");
		CPPUNIT_ASSERT(!s.pull(output));
		CPPUNIT_ASSERT(output == "hello, world");
	}

	void test_buffer_sink()
	{
		using namespace x0;

		buffer_source src(const_buffer("Hello World!"));
		buffer_sink snk;
		src.pump(snk);

		CPPUNIT_ASSERT(snk.buffer() == "Hello World!");
	}

	void test_fd_sink()
	{
		//! \todo
	}

	void test_file_sink()
	{
		//! \todo
	}

private:
private: // {{{ debug helper
	void print(const x0::buffer& b, const char *msg = 0)
	{
		if (msg && *msg)
			printf("buffer(%s): '%s'\n", msg, b.str().c_str());
		else
			printf("buffer: '%s'\n", b.str().c_str());
	}

	void print(const x0::buffer_ref& v, const char *msg = 0)
	{
		char prefix[64];
		if (msg && *msg)
			snprintf(prefix, sizeof(prefix), "buffer.view(%s)", msg);
		else
			snprintf(prefix, sizeof(prefix), "buffer.view");

		if (v)
			printf("%s: '%s' (offset=%ld, size=%ld)\n",
				prefix, v.str().c_str(), v.offset(), v.size());
		else
			printf("%s: NULL\n", prefix);
	}
	//}}}
};

CPPUNIT_TEST_SUITE_REGISTRATION(io_test);
