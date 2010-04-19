#include <x0/response_parser.hpp>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

using namespace x0;
using namespace std::placeholders;

#if !CC_SUPPORTS_LAMBDA
#	warning "Compiler does not support lambda expressions. Cannot unit-test response_parser"
#else

class response_parser_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(response_parser_test);
		CPPUNIT_TEST(simple);
		CPPUNIT_TEST(no_status_text);
		CPPUNIT_TEST(no_header);
		CPPUNIT_TEST(pipeline);
		CPPUNIT_TEST(chunked_body);
		CPPUNIT_TEST(content_length);
	CPPUNIT_TEST_SUITE_END();

private:
	void pipeline()
	{
	}

	void chunked_body()
	{
		buffer r1(
			"HTTP/1.1 200 Ok\r\n"
			"Name: Value\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nsome\r\n"
			"1\r\n \r\n"
			"4\r\nbody\r\n"
			"0\r\n\r\n"
			//"GARBAGE"
		);
		response_parser rp;

		rp.on_content = [&](const buffer_ref& chunk)
		{
			printf("chunked_body(%s)\n", chunk.str().c_str());
			CPPUNIT_ASSERT(equals(chunk, "some body"));
		};

		std::size_t rv = rp.parse(r1);
		CPPUNIT_ASSERT(rv == r1.size());
	}

	void content_length()
	{
		buffer r1(
			"HTTP/1.1 200 Ok\r\n"
			"Name: Value\r\n"
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
			"GARBAGE"
		);

		response_parser rp;

		rp.on_content = [&](const buffer_ref& chunk)
		{
			CPPUNIT_ASSERT(equals(chunk, "some body"));
		};
		rp.on_complete = [&]()
		{
			return false;
		};

		std::size_t rv = rp.parse(r1);
		CPPUNIT_ASSERT(rv == r1.size() - 7);
	}

	void simple()
	{
		int header_count = 0;
		int body_count = 0;
		response_parser rp;

		rp.on_status = [&](const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
		{
			//printf("status('%s', '%s', '%s')\n", protocol.str().c_str(), code.str().c_str(), text.str().c_str());
			CPPUNIT_ASSERT(protocol == "HTTP/1.1");
			CPPUNIT_ASSERT(code == "200");
			CPPUNIT_ASSERT(text == "Ok");
		};

		rp.on_header = [&](const buffer_ref& name, const buffer_ref& value)
		{
			//printf("on_header('%s', '%s')\n", name.str().c_str(), value.str().c_str());

			switch (++header_count)
			{
			case 1:
				CPPUNIT_ASSERT(name == "Name");
				CPPUNIT_ASSERT(value == "Value");
				break;
			case 2:
				CPPUNIT_ASSERT(name == "Name 2");
				CPPUNIT_ASSERT(value == "Value 2");
				break;
			default:
				CPPUNIT_ASSERT(0 == "invalid header count");
			}
		};

		rp.on_content = [&](const buffer_ref& content)
		{
			//printf("on_content(%ld): '%s'\n", content.size(), content.str().c_str());
			CPPUNIT_ASSERT(++body_count == 1);
			CPPUNIT_ASSERT(content == "some body");
		};

		buffer r1(
			"HTTP/1.1 200 Ok\r\n"
			"Name: Value\r\n"
			"Name 2: Value 2\r\n"
			"\r\n"
			"some body"
		);
		rp.parse(r1);
		CPPUNIT_ASSERT(body_count == 1);
	}

	void no_status_text()
	{
		int header_count = 0;
		int body_count = 0;
		response_parser rp;

		rp.on_status = [&](const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
		{
			CPPUNIT_ASSERT(protocol == "HTTP/1.1");
			CPPUNIT_ASSERT(code == "200");
			CPPUNIT_ASSERT(text == "");
		};

		rp.on_header = [&](const buffer_ref& name, const buffer_ref& value)
		{
			CPPUNIT_ASSERT(++header_count == 1);
			CPPUNIT_ASSERT(name == "Name");
			CPPUNIT_ASSERT(value == "Value");
		};

		rp.on_content = [&](const buffer_ref& content)
		{
			CPPUNIT_ASSERT(++body_count == 1);
			CPPUNIT_ASSERT(content == "some body");
		};

		buffer r1(
			"HTTP/1.1 200\r\n"
			"Name: Value\r\n"
			"\r\n"
			"some body"
		);
		rp.parse(r1);

		CPPUNIT_ASSERT(body_count == 1);
	}

	void no_header()
	{
		response_parser rp;

		rp.on_status = [&](const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
		{
			CPPUNIT_ASSERT(protocol == "HTTP/1.1");
			CPPUNIT_ASSERT(code == "200");
			CPPUNIT_ASSERT(text == "");
		};

		rp.on_header = [&](const buffer_ref& name, const buffer_ref& value)
		{
			CPPUNIT_ASSERT(0 == "there shall be no headers");
		};

		rp.on_content = [&](const buffer_ref& content)
		{
			CPPUNIT_ASSERT(content == "some body");
		};

		buffer r1(
			"HTTP/1.1 200\r\n"
			"\r\n"
			"some body"
		);
		rp.parse(r1);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(response_parser_test);
#endif
