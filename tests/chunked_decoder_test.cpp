#include <x0/io/chunked_decoder.hpp>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

using namespace x0;
using namespace std::placeholders;

class chunked_decoder_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(chunked_decoder_test);
		CPPUNIT_TEST(simple);
	CPPUNIT_TEST_SUITE_END();

private:
	void simple()
	{
		CPPUNIT_ASSERT(equals(decode("0\r\n\r\n"), ""));
		CPPUNIT_ASSERT(equals(decode("b\r\nhello world\r\n"), "hello world"));
		CPPUNIT_ASSERT(equals(decode("5\r\nhello\r\n1\r\n \r\n5\r\nworld\r\n"), "hello world"));
	}

private:
	template<typename T>
	buffer decode(T&& value)
	{
		chunked_decoder filter;
		return filter.process(x0::buffer(value).ref());
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(chunked_decoder_test);
