#include <x0/buffer.hpp>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

class buffer_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(buffer_test);
		// buffer
		CPPUNIT_TEST(ctor0);
		CPPUNIT_TEST(const_buffer1);
		CPPUNIT_TEST(resize);
		CPPUNIT_TEST(capacity);
		CPPUNIT_TEST(reserve);
		CPPUNIT_TEST(clear);
		CPPUNIT_TEST(operator_bool);
		CPPUNIT_TEST(operator_not);
		CPPUNIT_TEST(iterators);
		CPPUNIT_TEST(const_iterators);
		CPPUNIT_TEST(push_back);
		CPPUNIT_TEST(random_access);
		CPPUNIT_TEST(ref);
		CPPUNIT_TEST(call);
		CPPUNIT_TEST(std_string);

		// buffer::view
		CPPUNIT_TEST(view_ctor);
		CPPUNIT_TEST(view_begins);
	CPPUNIT_TEST_SUITE_END();

private:
	// {{{ buffer tests
	void ctor0()
	{
		x0::buffer a;

		CPPUNIT_ASSERT(a.empty());
		CPPUNIT_ASSERT(a.size() == 0);
		CPPUNIT_ASSERT(!a);
		CPPUNIT_ASSERT(!static_cast<bool>(a));
	}

	void const_buffer1()
	{
		x0::const_buffer empty("");
		CPPUNIT_ASSERT(empty.empty());
		CPPUNIT_ASSERT(empty.size() == 0);
		CPPUNIT_ASSERT(empty == "");

		x0::const_buffer hello("hello");
		CPPUNIT_ASSERT(!hello.empty());
		CPPUNIT_ASSERT(hello.size() == 5);
		CPPUNIT_ASSERT(hello == "hello");

		//! \todo ensure const buffers are unmutable
	}

	void resize()
	{
		// test modifying buffer size
		x0::buffer buf;
		buf.push_back("hello");
		CPPUNIT_ASSERT(buf.size() == 5);

		buf.resize(4);
		CPPUNIT_ASSERT(buf.size() == 4);
		CPPUNIT_ASSERT(buf == "hell");

		//! \todo should not be resizable (const_buffer)
		//x0::const_buffer cbuf("hello");
		//cbuf.size(4);
	}

	// test modifying capacity
	void capacity()
	{
		x0::buffer buf;
		CPPUNIT_ASSERT(buf.capacity() == 0);

		buf.push_back("hello");
		CPPUNIT_ASSERT(buf.capacity() >= 5);

		buf.capacity(4);
		CPPUNIT_ASSERT(buf.capacity() == 4);
		CPPUNIT_ASSERT(buf.size() == 4);
		CPPUNIT_ASSERT(buf == "hell");
	}

	// test reserve()
	void reserve()
	{
		x0::buffer buf;

		buf.reserve(1);

		CPPUNIT_ASSERT(buf.size() == 0);
		CPPUNIT_ASSERT(buf.capacity() == x0::buffer::CHUNK_SIZE);
	}

	void clear()
	{
		x0::buffer buf;
		buf.push_back("hello");

		const std::size_t capacity = buf.capacity();

		buf.clear();
		CPPUNIT_ASSERT(buf.empty());
		CPPUNIT_ASSERT(buf.size() == 0);

		// shouldn't have changed internal buffer
		CPPUNIT_ASSERT(buf.capacity() == capacity);
	}

	void operator_bool()
	{
		x0::buffer buf;
		CPPUNIT_ASSERT(bool(buf) == false);

		buf.push_back("hello");
		CPPUNIT_ASSERT(bool(buf) == true);
	}

	void operator_not()
	{
		x0::buffer buf;
		CPPUNIT_ASSERT(buf.operator!() == true);

		buf.push_back("hello");
		CPPUNIT_ASSERT(buf.operator!() == false);
	}

	void iterators()
	{
		{
			x0::buffer buf;
			buf.push_back("hello");

			x0::buffer::iterator i = buf.begin();
			CPPUNIT_ASSERT(i != buf.end());
			CPPUNIT_ASSERT(*i == 'h');
			CPPUNIT_ASSERT(*++i == 'e');
			CPPUNIT_ASSERT(i != buf.end());
			CPPUNIT_ASSERT(*i++ == 'e');
			CPPUNIT_ASSERT(*i == 'l');
			CPPUNIT_ASSERT(i != buf.end());
			CPPUNIT_ASSERT(*++i == 'l');
			CPPUNIT_ASSERT(i != buf.end());
			CPPUNIT_ASSERT(*++i == 'o');
			CPPUNIT_ASSERT(i != buf.end());
			CPPUNIT_ASSERT(++i == buf.end());
		}

		{
			x0::buffer buf;
			buf.push_back("hello");

			for (x0::buffer::iterator i = buf.begin(); i != buf.end(); ++i)
				*i = std::toupper(*i);

			CPPUNIT_ASSERT(buf == "HELLO");
		}
	}

	void const_iterators()
	{
		x0::buffer buf;
		buf.push_back("hello");

		x0::const_buffer::const_iterator i = buf.cbegin();
		CPPUNIT_ASSERT(i != buf.cend());
		CPPUNIT_ASSERT(*i == 'h');
		CPPUNIT_ASSERT(*++i == 'e');
		CPPUNIT_ASSERT(i != buf.cend());
		CPPUNIT_ASSERT(*i++ == 'e');
		CPPUNIT_ASSERT(*i == 'l');
		CPPUNIT_ASSERT(i != buf.cend());
		CPPUNIT_ASSERT(*++i == 'l');
		CPPUNIT_ASSERT(i != buf.cend());
		CPPUNIT_ASSERT(*++i == 'o');
		CPPUNIT_ASSERT(i != buf.cend());
		CPPUNIT_ASSERT(++i == buf.cend());
	}

	void push_back()
	{
		x0::buffer buf;

		buf.push_back('h');
		CPPUNIT_ASSERT(buf == "h");


		buf.push_back("");
		CPPUNIT_ASSERT(buf == "h");
		buf.push_back("e");
		CPPUNIT_ASSERT(buf == "he");

		buf.push_back("llo");
		CPPUNIT_ASSERT(buf == "hello");

		std::string s(" world");
		buf.push_back(s);
		CPPUNIT_ASSERT(buf == "hello world");

		buf.clear();
		buf.push_back(s.data(), s.size());
		CPPUNIT_ASSERT(buf == " world");
	}

	void random_access()
	{
		// test operator[](...)
	}

	void ref()
	{
		x0::const_buffer a("hello");

		CPPUNIT_ASSERT(a == "hello");
		CPPUNIT_ASSERT(a.ref(0) == "hello");
		CPPUNIT_ASSERT(a.ref(1) == "ello");
		CPPUNIT_ASSERT(a.ref(2) == "llo");
		CPPUNIT_ASSERT(a.ref(5) == "");
	}

	void call()
	{
		x0::const_buffer a("hello");

		CPPUNIT_ASSERT(a() == "hello");
		CPPUNIT_ASSERT(a(0) == "hello");
		CPPUNIT_ASSERT(a(1) == "ello");
		CPPUNIT_ASSERT(a(2) == "llo");
		CPPUNIT_ASSERT(a(5) == "");
	}

	void std_string()
	{
		// test std::string() utility functions

		x0::const_buffer a("hello");
		std::string s(a.str());

		CPPUNIT_ASSERT(a.data() != s.data());
		CPPUNIT_ASSERT(a.size() == s.size());
		CPPUNIT_ASSERT(s == "hello");
	}
	// }}}

	// {{{ buffer::view tests
	void view_ctor()
	{
	}

	void view_ctor1()
	{
	}

	void view_ctor_buffer()
	{
	}

	void view_ctor_ctor()
	{
	}

	void view_operator_asn_buffer()
	{
	}

	void view_operator_asn_view()
	{
	}

	void view_empty()
	{
	}

	void view_offset()
	{
	}

	void view_size()
	{
	}

	void view_data()
	{
	}

	void view_operator_bool()
	{
	}

	void view_operator_not()
	{
	}

	void view_iterator()
	{
	}

	void view_find_view()
	{
	}

	void view_find_value_ptr()
	{
	}

	void view_find_value()
	{
	}

	void view_rfind_view()
	{
	}

	void view_rfind_value_ptr()
	{
	}

	void view_rfind_value()
	{
	}

	void view_begins()
	{
		x0::const_buffer b("hello");
		x0::buffer::view v(b);

		CPPUNIT_ASSERT(v.begins((const char *)0));
		CPPUNIT_ASSERT(v.begins(""));
		CPPUNIT_ASSERT(v.begins("hello"));
	}

	void view_ibegins()
	{
	}
	// }}}

private: // {{{ debug helper
	void print(const x0::buffer& b, const char *msg = 0)
	{
		if (msg && *msg)
			printf("buffer(%s): '%s'\n", msg, b.str().c_str());
		else
			printf("buffer: '%s'\n", b.str().c_str());
	}

	void print(const x0::buffer::view& v, const char *msg = 0)
	{
		if (msg && *msg)
			printf("buffer.view(%s): '%s'\n", msg, v.str().c_str());
		else
			printf("buffer.view: '%s'\n", v.str().c_str());

		printf("  view.offset=%ld, size=%ld\n", v.offset(), v.size());
	}
	//}}}
};

CPPUNIT_TEST_SUITE_REGISTRATION(buffer_test);
