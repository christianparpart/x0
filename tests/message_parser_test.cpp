#include <x0/message_processor.hpp>
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
#	warning "Compiler does not support lambda expressions. Cannot unit-test message_processor"
#else

class message_processor_component : // {{{
	public x0::message_processor
{
public:
	message_processor_component(message_processor::mode_type mode) :
		message_processor(mode)
	{
	}

	std::error_code process(buffer_ref&& chunk)
	{
		std::size_t np = 0;
		return process(std::move(chunk), np);
	}

	std::error_code process(buffer_ref&& chunk, std::size_t& np)
	{
		std::error_code ec;
		ec = message_processor::process(std::move(chunk), np);

		if (ec)
			DEBUG("process: nparsed=%ld/%ld; state=%s; ec=%s; '%s'",
					np, chunk.size(), state_str(), ec.message().c_str(), chunk.begin() + np);
		else
			DEBUG("process: nparsed=%ld/%ld; state=%s; ec=%s",
					np, chunk.size(), state_str(), ec.message().c_str());

		return ec;
	}

	std::function<void(buffer_ref&&, buffer_ref&&, int, int)> on_request;
	std::function<void(int, int, int, buffer_ref&&)> on_status;
	std::function<void()> on_message;
	std::function<void(buffer_ref&&, buffer_ref&&)> on_header;
	std::function<bool()> on_header_done;
	std::function<bool(buffer_ref&&)> on_content;
	std::function<bool()> on_complete;

private:
	virtual void message_begin(buffer_ref&& method, buffer_ref&& uri, int version_major, int version_minor)
	{
		if (on_request)
			on_request(std::move(method), std::move(uri), version_major, version_minor);
	}

	virtual void message_begin(int version_major, int version_minor, int code, buffer_ref&& text)
	{
		if (on_status)
			on_status(version_major, version_minor, code, std::move(text));
	}

	virtual void message_begin()
	{
		if (on_message)
			on_message();
	}

	virtual void message_header(buffer_ref&& name, buffer_ref&& value)
	{
		if (on_header)
			on_header(std::move(name), std::move(value));
	}

	virtual bool message_header_done()
	{
		if (on_header_done)
			return on_header_done();

		return true;
	}

	virtual bool message_content(buffer_ref&& chunk)
	{
		if (on_content)
			return on_content(std::move(chunk));

		return true;
	}

	virtual bool message_end()
	{
		if (on_complete)
			return on_complete();

		return true;
	}
}; // }}}

class message_processor_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(message_processor_test);
		//CPPUNIT_TEST(request_simple);
		//CPPUNIT_TEST(request_complex_lws_headers);
		CPPUNIT_TEST(request_no_header);

		//CPPUNIT_TEST(response_simple);
		//CPPUNIT_TEST(response_sample_304);
		//CPPUNIT_TEST(response_no_status_text);

		//CPPUNIT_TEST(message_chunked_body);
		//CPPUNIT_TEST(message_content_length);
		//CPPUNIT_TEST(message_content_recursive);
	CPPUNIT_TEST_SUITE_END();

private:
	void request_complex_lws_headers()
	{
		message_processor_component rp(message_processor::REQUEST);

		buffer r(
			"GET /foo HTTP/1.1\r\n"
			"Single-Line: single value\r\n"            // simple LWS at the beginning & middle
			"Multi-Line-1: multi\r\n\tvalue 1\r\n"     // complex LWS in the middle
			"Multi-Line-2:\r\n \t \tmulti value 2\r\n" // complex LWS at the beginning
			"\r\n"
		);
		rp.process(r);
	}

	void request_simple()
	{
		message_processor_component rp(message_processor::REQUEST); // (message_processor::REQUEST);

		rp.on_request = [&](buffer_ref&& method, buffer_ref&& entity, int major, int minor)
		{
			CPPUNIT_ASSERT(equals(method, "GET"));
			CPPUNIT_ASSERT(equals(entity, "/"));
			CPPUNIT_ASSERT(major == 1);
			CPPUNIT_ASSERT(minor== 1);
		};

		int header_count = 0;
		rp.on_header = [&](buffer_ref&& name, buffer_ref&& value)
		{
			switch (++header_count)
			{
				case 1:
					CPPUNIT_ASSERT(equals(name, "foo"));
					CPPUNIT_ASSERT(equals(value, "bar"));
					break;
				case 2:
					CPPUNIT_ASSERT(equals(name, "Content-Length"));
					CPPUNIT_ASSERT(equals(value, "11"));
					break;
				default:
					CPPUNIT_ASSERT(0 == "too many invokations");
			}
		};

		int chunk_count = 0;
		rp.on_content = [&](buffer_ref&& chunk)
		{
			++chunk_count;
			CPPUNIT_ASSERT(chunk_count == 1);
			CPPUNIT_ASSERT(equals(chunk, "hello world"));
			return true;
		};

		buffer r(
			"GET / HTTP/1.1\r\n"
			"foo: bar\r\n"
			"Content-Length: 11\r\n"
			"\r\n"
			"hello world"
		);

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size());
		CPPUNIT_ASSERT(!ec);
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

		message_processor_component rp(message_processor::RESPONSE);
		bool on_complete_invoked = false;

		rp.on_complete = [&]()
		{
			on_complete_invoked = true;
			return true;
		};

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(!ec);
		CPPUNIT_ASSERT(np == r.size());
		CPPUNIT_ASSERT(on_complete_invoked == true);
	}

	void response_simple()
	{
		int header_count = 0;
		int body_count = 0;
		message_processor_component rp(message_processor::RESPONSE);

		rp.on_status = [&](int vmajor, int vminor, int code, const buffer_ref& text)
		{
			CPPUNIT_ASSERT(vmajor == 1);
			CPPUNIT_ASSERT(vminor == 1);
			CPPUNIT_ASSERT(code == 200);
			CPPUNIT_ASSERT(text == "Ok");
		};

		rp.on_header = [&](const buffer_ref& name, const buffer_ref& value)
		{
			switch (++header_count)
			{
			case 1:
				CPPUNIT_ASSERT(name == "Name");
				CPPUNIT_ASSERT(value == "Value");
				break;
			case 2:
				CPPUNIT_ASSERT(name == "Name-2");
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
			CPPUNIT_ASSERT(++body_count == 1);
			CPPUNIT_ASSERT(content == "some-body");
			return true;
		};

		buffer r(
			"HTTP/1.1 200 Ok\r\n"
			"Name: Value\r\n"
			"Name-2: Value 2\r\n"
			"Content-Length: 9\r\n"
			"\r\n"
			"some-body"
		);

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size());
		CPPUNIT_ASSERT(body_count == 1);
	}

	void response_no_status_text()
	{
		int header_count = 0;
		int body_count = 0;
		message_processor_component rp(message_processor::RESPONSE);

		rp.on_status = [&](int vmajor, int vminor, int code, const buffer_ref& text)
		{
			CPPUNIT_ASSERT(vmajor == 1);
			CPPUNIT_ASSERT(vminor == 1);
			CPPUNIT_ASSERT(code == 200);
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
			return true;
		};

		buffer r(
			"HTTP/1.1 200\r\n"
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
		);

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np = r.size());
		CPPUNIT_ASSERT(body_count == 1);
	}

	void request_no_header()
	{
		message_processor_component rp(message_processor::REQUEST);

		int request_count = 0;
		rp.on_request = [&](buffer_ref&& method, buffer_ref&& url, int major, int minor)
		{
			switch (++request_count)
			{
				case 1:
					CPPUNIT_ASSERT(method == "GET");
					CPPUNIT_ASSERT(url == "/");
					CPPUNIT_ASSERT(major == 1);
					CPPUNIT_ASSERT(minor == 1);
					break;
				case 2:
					CPPUNIT_ASSERT(method == "DELETE");
					CPPUNIT_ASSERT(url == "/foo/bar");
					CPPUNIT_ASSERT(major == 1);
					CPPUNIT_ASSERT(minor == 1);
					break;
				default:
					CPPUNIT_ASSERT(0 == "Too many requests.");
					break;
			}
		};

		rp.on_header = [&](const buffer_ref& name, const buffer_ref& value)
		{
			CPPUNIT_ASSERT(0 == "no headers expected");
		};

		int on_header_done_invoked = 0;
		rp.on_header_done = [&]()
		{
			++on_header_done_invoked;
			return true;
		};

		rp.on_content = [&](const buffer_ref& content)
		{
			CPPUNIT_ASSERT(0 == "no content expected");
			return true;
		};

		buffer r(
			"GET / HTTP/1.1\r\n"
			"\r\n"
			"DELETE /foo/bar HTTP/1.1\r\n"
			"\r\n"
		);

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size());
		CPPUNIT_ASSERT(!ec);
	}

private: // message tests
	void message_chunked_body()
	{
		buffer r(
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nsome\r\n"
			"1\r\n \r\n"
			"4\r\nbody\r\n"
			"0\r\n\r\n"
			"GARBAGE"
		);
		message_processor_component rp(message_processor::MESSAGE);

		int chunk_index = 0;
		rp.on_content = [&](const buffer_ref& chunk)
		{
			switch (chunk_index++)
			{
				case 0:
					CPPUNIT_ASSERT(equals(chunk, "some"));
					break;
				case 1:
					CPPUNIT_ASSERT(equals(chunk, " "));
					break;
				case 2:
					CPPUNIT_ASSERT(equals(chunk, "body"));
					break;
				default:
					CPPUNIT_ASSERT(0 == "Too many chunks.");
			}
			return true;
		};
		rp.on_complete = [&]()
		{
			return false;
		};

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size() - 7);
		CPPUNIT_ASSERT(ec == http_message_error::aborted);
	}

	void message_content_length()
	{
		buffer r(
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
			"GARBAGE"
		);

		message_processor_component rp(message_processor::MESSAGE);

		rp.on_content = [&](const buffer_ref& chunk)
		{
			CPPUNIT_ASSERT(equals(chunk, "some body"));
			return true;
		};
		rp.on_complete = [&]()
		{
			return false;
		};

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size() - 7);
		CPPUNIT_ASSERT(ec == http_message_error::aborted);
	}

	void message_content_recursive()
	{
		buffer r(
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
		);

		message_processor_component rp(message_processor::MESSAGE);

		rp.on_header_done = [&]()
		{
			std::size_t np = 0;
			std::error_code ec = rp.process(r.ref(rp.next_offset(), r.size() - rp.next_offset()), np);

			CPPUNIT_ASSERT(np == 9);
			CPPUNIT_ASSERT(!ec);

			return false;
		};

		rp.on_content = [&](const buffer_ref& chunk)
		{
			DEBUG("on_content('%s')", chunk.str().c_str());
			CPPUNIT_ASSERT(equals(chunk, "some body"));
			return true;
		};

		rp.on_complete = [&]()
		{
			return true;
		};

		std::size_t np = 0;
		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size() - 9);
		CPPUNIT_ASSERT(ec == http_message_error::aborted); // cancelled parsing at header-done state
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(message_processor_test);
#endif
