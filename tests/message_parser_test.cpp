#include <x0/message_parser.hpp>
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
#	warning "Compiler does not support lambda expressions. Cannot unit-test message_parser"
#else

class message_parser_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(message_parser_test);
		CPPUNIT_TEST(request_simple);
		CPPUNIT_TEST(request_chunked_body);
		//CPPUNIT_TEST(request_content_length);

#if 0
		CPPUNIT_TEST(response_simple);
		CPPUNIT_TEST(response_sample_304);
		CPPUNIT_TEST(response_no_status_text);
		CPPUNIT_TEST(response_no_header);
		CPPUNIT_TEST(response_chunked_body);
		CPPUNIT_TEST(response_content_length);
#endif
	CPPUNIT_TEST_SUITE_END();

private:
	void request_simple()
	{
		message_parser rp; // (message_parser::REQUEST);

		rp.on_request = [&](buffer_ref&& method, buffer_ref&& entity, buffer_ref&& protocol, int major, int minor) {
			printf("on_response('%s', '%s', '%s', %d, %d)\n",
			       method.str().c_str(), entity.str().c_str(), protocol.str().c_str(), major, minor);
		};
		rp.on_header = [&](buffer_ref&& name, buffer_ref&& value) {
			printf("on_header('%s', '%s')\n", name.str().c_str(), value.str().c_str());
		};
		rp.on_content = [&](buffer_ref&& chunk) {
			printf("on_content(%ld): '%s'\n", chunk.size(), chunk.str().c_str());
		};

		buffer r(
			"GET /foo HTTP/12.34\r\n"
			"foo: bar\r\n"
			"Content-Length: 11\r\n"
			"\r\n"
			"hello world"
		);
		std::error_code ec;
		std::size_t nparsed = rp.parse(r, ec);
		printf("nparsed: %ld; error: %s\n", nparsed, ec.message().c_str());
	}

	void request_chunked_body()
	{
		buffer r(
			"PUT /blah HTTP/1.1\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nsome\r\n"
			"1\r\n \r\n"
			"4\r\nbody\r\n"
			"0\r\n\r\n"
			//"GARBAGE"
		);
		message_parser rp;

		rp.on_content = [&](const buffer_ref& chunk)
		{
			printf("chunked_body(%s)\n", chunk.str().c_str());
			CPPUNIT_ASSERT(equals(chunk, "some body"));
		};

		std::size_t rv = rp.parse(r);
		CPPUNIT_ASSERT(rv == r.size());
	}

	void response_chunked_body()
	{
		buffer r(
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
		message_parser rp;

		rp.on_content = [&](const buffer_ref& chunk)
		{
			printf("chunked_body(%s)\n", chunk.str().c_str());
			CPPUNIT_ASSERT(equals(chunk, "some body"));
		};

		std::size_t rv = rp.parse(r);
		CPPUNIT_ASSERT(rv == r.size());
	}

	void response_sample_304()
	{
		buffer r(
			"HTTP/1.1 304 Not Modified\r\n"
			"Date: Mon, 19 Apr 2010 14:56:34 GMT\r\n"
			"Server: Apache\r\n"
			"Connection: close\r\n"
			"ETag: \"37210c-33b5-483 1136540000\"\r\n"
			"\r\n"
		);

		message_parser rp;
		bool on_complete_invoked = false;

		rp.on_complete = [&]()
		{
			on_complete_invoked = true;
			return true;
		};

		std::size_t nparsed = rp.parse(r);

		CPPUNIT_ASSERT(nparsed == r.size());
		CPPUNIT_ASSERT(on_complete_invoked == true);
	}

	void response_content_length()
	{
		buffer r(
			"HTTP/1.1 200 Ok\r\n"
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
			"GARBAGE"
		);

		message_parser rp;

		rp.on_content = [&](const buffer_ref& chunk)
		{
			CPPUNIT_ASSERT(equals(chunk, "some body"));
		};
		rp.on_complete = [&]()
		{
			return false;
		};

		std::size_t rv = rp.parse(r);
		CPPUNIT_ASSERT(rv == r.size() - 7);
	}

	void response_simple()
	{
		int header_count = 0;
		int body_count = 0;
		message_parser rp;

		rp.on_response = [&](const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
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
			case 3:
				CPPUNIT_ASSERT(name == "Content-Length");
				CPPUNIT_ASSERT(value == "9");
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

		buffer r(
			"HTTP/1.1 200 Ok\r\n"
			"Name: Value\r\n"
			"Name 2: Value 2\r\n"
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
		);
		rp.parse(r);
		CPPUNIT_ASSERT(body_count == 1);
	}

	void response_no_status_text()
	{
		int header_count = 0;
		int body_count = 0;
		message_parser rp;

		rp.on_response = [&](const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
		{
			CPPUNIT_ASSERT(protocol == "HTTP/1.1");
			CPPUNIT_ASSERT(code == "200");
			CPPUNIT_ASSERT(text == "");
		};

		rp.on_header = [&](const buffer_ref& name, const buffer_ref& value)
		{
			CPPUNIT_ASSERT(++header_count == 1);
			CPPUNIT_ASSERT(name == "Content-Length");
			CPPUNIT_ASSERT(value == "9");
		};

		rp.on_content = [&](const buffer_ref& content)
		{
			CPPUNIT_ASSERT(++body_count == 1);
			CPPUNIT_ASSERT(content == "some body");
		};

		buffer r(
			"HTTP/1.1 200\r\n"
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
		);
		rp.parse(r);

		CPPUNIT_ASSERT(body_count == 1);
	}

	void response_no_header()
	{
		message_parser rp;

		rp.on_response = [&](const buffer_ref& protocol, const buffer_ref& code, const buffer_ref& text)
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

		buffer r(
			"HTTP/1.1 200\r\n"
			"\r\n"
			"some body"
		);
		rp.parse(r);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(message_parser_test);
#endif
