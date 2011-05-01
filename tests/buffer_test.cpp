#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/ConstBuffer.h>
#include <x0/FixedBuffer.h>
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
		// Buffer
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

		// FixedBuffer
		// TODO
		CPPUNIT_TEST(fixed_buf);

		// ConstBuffer
		// TODO

		// BufferRef
		CPPUNIT_TEST(ref_ctor);
		CPPUNIT_TEST(ref_begins);
		CPPUNIT_TEST(ref_find_value_ptr);
		CPPUNIT_TEST(ref_as_bool);
		CPPUNIT_TEST(ref_as_int);
		CPPUNIT_TEST(ref_hex);
	CPPUNIT_TEST_SUITE_END();

private:
	// {{{ buffer tests
	void ctor0()
	{
		x0::Buffer a;

		CPPUNIT_ASSERT(a.empty());
		CPPUNIT_ASSERT(a.size() == 0);
		CPPUNIT_ASSERT(!a);
		CPPUNIT_ASSERT(!static_cast<bool>(a));
	}

	x0::Buffer getbuf()
	{
		//x0::Buffer buf;
		//buf.push_back("12345");
		//return buf;
		return x0::Buffer("12345");
	}

	void const_buffer1()
	{
		x0::ConstBuffer empty("");
		CPPUNIT_ASSERT(empty.empty());
		CPPUNIT_ASSERT(empty.size() == 0);
		CPPUNIT_ASSERT(empty == "");

		x0::ConstBuffer hello("hello");
		CPPUNIT_ASSERT(!hello.empty());
		CPPUNIT_ASSERT(hello.size() == 5);
		CPPUNIT_ASSERT(hello == "hello");

		//! \todo ensure const buffers are unmutable
	}

	void resize()
	{
		// test modifying buffer size
		x0::Buffer buf;
		buf.push_back("hello");
		CPPUNIT_ASSERT(buf.size() == 5);

		buf.resize(4);
		CPPUNIT_ASSERT(buf.size() == 4);
		CPPUNIT_ASSERT(buf == "hell");

		//! \todo should not be resizable (const_buffer)
		//x0::ConstBuffer cbuf("hello");
		//cbuf.size(4);
	}

	// test modifying capacity
	void capacity()
	{
		x0::Buffer buf;
		CPPUNIT_ASSERT(buf.capacity() == 0);

		buf.push_back("hello");
		CPPUNIT_ASSERT(buf.capacity() >= 5);

		buf.setCapacity(4);
		CPPUNIT_ASSERT(buf.capacity() >= 4);
		CPPUNIT_ASSERT(buf.size() == 4);
		CPPUNIT_ASSERT(buf == "hell");
	}

	// test reserve()
	void reserve()
	{
		x0::Buffer buf;

		buf.reserve(1);

		CPPUNIT_ASSERT(buf.size() == 0);
		CPPUNIT_ASSERT(buf.capacity() == x0::Buffer::CHUNK_SIZE);
	}

	void clear()
	{
		x0::Buffer buf;
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
		x0::Buffer buf;
		CPPUNIT_ASSERT(bool(buf) == false);

		buf.push_back("hello");
		CPPUNIT_ASSERT(bool(buf) == true);
	}

	void operator_not()
	{
		x0::Buffer buf;
		CPPUNIT_ASSERT(buf.operator!() == true);

		buf.push_back("hello");
		CPPUNIT_ASSERT(buf.operator!() == false);
	}

	void iterators()
	{
		{
			x0::Buffer buf;
			buf.push_back("hello");

			x0::Buffer::iterator i = buf.begin();
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
			x0::Buffer buf;
			buf.push_back("hello");

			for (x0::Buffer::iterator i = buf.begin(); i != buf.end(); ++i)
				*i = std::toupper(*i);

			CPPUNIT_ASSERT(buf == "HELLO");
		}
	}

	void const_iterators()
	{
		x0::Buffer buf;
		buf.push_back("hello");

		x0::ConstBuffer::const_iterator i = buf.cbegin();
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
		x0::Buffer buf;

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
		x0::ConstBuffer a("hello");

		CPPUNIT_ASSERT(a == "hello");
		CPPUNIT_ASSERT(a.ref(0) == "hello");
		CPPUNIT_ASSERT(a.ref(1) == "ello");
		CPPUNIT_ASSERT(a.ref(2) == "llo");
		CPPUNIT_ASSERT(a.ref(5) == "");
	}

	void call()
	{
		x0::ConstBuffer a("hello");

		CPPUNIT_ASSERT(a() == "hello");
		CPPUNIT_ASSERT(a(0) == "hello");
		CPPUNIT_ASSERT(a(1) == "ello");
		CPPUNIT_ASSERT(a(2) == "llo");
		CPPUNIT_ASSERT(a(5) == "");
	}

	void std_string()
	{
		// test std::string() utility functions

		x0::ConstBuffer a("hello");
		std::string s(a.str());

		CPPUNIT_ASSERT(a.data() != s.data());
		CPPUNIT_ASSERT(a.size() == s.size());
		CPPUNIT_ASSERT(s == "hello");
	}
	// }}}

	// {{{ FixedBuffer
	void fixed_buf()
	{
		x0::FixedBuffer<2> b2;
		b2.push_back('1');
		CPPUNIT_ASSERT(b2.size() == 1);

		b2.push_back('2');
		CPPUNIT_ASSERT(b2.size() == 2);

		b2.push_back('3');
		CPPUNIT_ASSERT(b2.size() == 2);
	}
	// }}}

	// {{{ BufferRef tests
	void ref_ctor()
	{
	}

	void ref_ctor1()
	{
	}

	void ref_ctor_buffer()
	{
	}

	void ref_ctor_ctor()
	{
	}

	void ref_operator_asn_buffer()
	{
	}

	void ref_operator_asn_view()
	{
	}

	void ref_empty()
	{
	}

	void ref_size()
	{
	}

	void ref_data()
	{
	}

	void ref_operator_bool()
	{
	}

	void ref_operator_not()
	{
	}

	void ref_iterator()
	{
	}

	void ref_find_ref()
	{
	}

	void ref_find_value_ptr()
	{
		x0::ConstBuffer buf("012345");
		x0::BufferRef ref = buf.ref(1);

		int i = ref.find("34");
		CPPUNIT_ASSERT(i == 2);

		CPPUNIT_ASSERT(ref.find("1") == 0);
		CPPUNIT_ASSERT(ref.find("12") == 0);
		CPPUNIT_ASSERT(ref.find("12345") == 0);
		CPPUNIT_ASSERT(ref.find("11") == x0::BufferRef::npos);
	}

	void ref_find_value()
	{
	}

	void ref_rfind_view()
	{
	}

	void ref_rfind_value_ptr()
	{
	}

	void ref_rfind_value()
	{
	}

	void ref_begins()
	{
		x0::ConstBuffer b("hello");
		x0::Buffer::view v(b);

		CPPUNIT_ASSERT(v.begins((const char *)0));
		CPPUNIT_ASSERT(v.begins(""));
		CPPUNIT_ASSERT(v.begins("hello"));
	}

	void ref_ibegins()
	{
	}

	void ref_as_bool()
	{
		// true
		CPPUNIT_ASSERT(x0::ConstBuffer("true").ref().as<bool>() == true);
		CPPUNIT_ASSERT(x0::ConstBuffer("TRUE").ref().as<bool>() == true);
		CPPUNIT_ASSERT(x0::ConstBuffer("True").ref().as<bool>() == true);
		CPPUNIT_ASSERT(x0::ConstBuffer("1").ref().as<bool>() == true);

		// false
		CPPUNIT_ASSERT(!x0::ConstBuffer("false").ref().as<bool>());
		CPPUNIT_ASSERT(!x0::ConstBuffer("FALSE").ref().as<bool>());
		CPPUNIT_ASSERT(!x0::ConstBuffer("False").ref().as<bool>());
		CPPUNIT_ASSERT(!x0::ConstBuffer("0").ref().as<bool>());

		// invalid cast results into false
		CPPUNIT_ASSERT(!x0::ConstBuffer("BLAH").ref().as<bool>());
	}

	void ref_as_int()
	{
		CPPUNIT_ASSERT(x0::ConstBuffer("1234").ref().as<int>() == 1234);

		CPPUNIT_ASSERT(x0::ConstBuffer("-1234").ref().as<int>() == -1234);
		CPPUNIT_ASSERT(x0::ConstBuffer("+1234").ref().as<int>() == +1234);

		CPPUNIT_ASSERT(x0::ConstBuffer("12.34").ref().as<int>() == 12);
	}

	void ref_hex()
	{
		CPPUNIT_ASSERT(x0::ConstBuffer("1234").ref().hex<int>() == 0x1234);
		CPPUNIT_ASSERT(x0::ConstBuffer("5678").ref().hex<int>() == 0x5678);

		CPPUNIT_ASSERT(x0::ConstBuffer("abcdef").ref().hex<int>() == 0xabcdef);
		CPPUNIT_ASSERT(x0::ConstBuffer("ABCDEF").ref().hex<int>() == 0xABCDEF);

		CPPUNIT_ASSERT(x0::ConstBuffer("ABCDEFG").ref().hex<int>() == 0xABCDEF);
		CPPUNIT_ASSERT(x0::ConstBuffer("G").ref().hex<int>() == 0);
	}
	// }}}

private: // {{{ debug helper
	void print(const x0::Buffer& b, const char *msg = 0)
	{
		if (msg && *msg)
			printf("buffer(%s): '%s'\n", msg, b.str().c_str());
		else
			printf("buffer: '%s'\n", b.str().c_str());
	}

	void print(const x0::Buffer::view& v, const char *msg = 0)
	{
		if (msg && *msg)
			printf("buffer.view(%s): '%s'\n", msg, v.str().c_str());
		else
			printf("buffer.view: '%s'\n", v.str().c_str());

		printf("  size=%ld\n", v.size());
	}
	//}}}
};

CPPUNIT_TEST_SUITE_REGISTRATION(buffer_test);
