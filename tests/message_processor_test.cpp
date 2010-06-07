#include <x0/http/HttpMessageProcessor.h>
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
#	warning "Compiler does not support lambda expressions. Cannot unit-test HttpMessageProcessor"
#else

class HttpMessageProcessor_component : // {{{
	public x0::HttpMessageProcessor
{
public:
	HttpMessageProcessor_component(HttpMessageProcessor::mode_type mode) :
		HttpMessageProcessor(mode)
	{
	}

	std::error_code process(BufferRef&& chunk)
	{
		std::size_t np = 0;
		return process(std::move(chunk), np);
	}

	std::error_code process(BufferRef&& chunk, std::size_t& np)
	{
		std::error_code ec;
		ec = HttpMessageProcessor::process(std::move(chunk), np);

		if (ec)
			DEBUG("process: nparsed=%ld/%ld; state=%s; ec=%s; '%s'",
					np, chunk.size(), state_str(), ec.message().c_str(), chunk.begin() + np);
		else
			DEBUG("process: nparsed=%ld/%ld; state=%s; ec=%s",
					np, chunk.size(), state_str(), ec.message().c_str());

		return ec;
	}

	std::function<void(BufferRef&&, BufferRef&&, int, int)> on_request;
	std::function<void(int, int, int, BufferRef&&)> on_status;
	std::function<void()> on_message;
	std::function<void(BufferRef&&, BufferRef&&)> on_header;
	std::function<bool()> on_header_done;
	std::function<bool(BufferRef&&)> on_content;
	std::function<bool()> on_complete;

private:
	virtual void message_begin(BufferRef&& method, BufferRef&& uri, int version_major, int version_minor)
	{
		if (on_request)
			on_request(std::move(method), std::move(uri), version_major, version_minor);
	}

	virtual void message_begin(int version_major, int version_minor, int code, BufferRef&& text)
	{
		if (on_status)
			on_status(version_major, version_minor, code, std::move(text));
	}

	virtual void message_begin()
	{
		if (on_message)
			on_message();
	}

	virtual void message_header(BufferRef&& name, BufferRef&& value)
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

	virtual bool message_content(BufferRef&& chunk)
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

class HttpMessageProcessor_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(HttpMessageProcessor_test);
		CPPUNIT_TEST(request_simple);
		CPPUNIT_TEST(request_complex_lws_headers);
		CPPUNIT_TEST(request_no_header);

		CPPUNIT_TEST(response_simple);
		CPPUNIT_TEST(response_sample_304);
		CPPUNIT_TEST(response_no_status_text);

		CPPUNIT_TEST(message_chunked_body);
		CPPUNIT_TEST(message_chunked_body_fragmented);
		CPPUNIT_TEST(message_content_length);
		CPPUNIT_TEST(message_content_recursive);
		CPPUNIT_TEST(message_multi);
	CPPUNIT_TEST_SUITE_END();

private:
	void request_complex_lws_headers()
	{
		HttpMessageProcessor_component rp(HttpMessageProcessor::REQUEST);

		Buffer r(
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
		HttpMessageProcessor_component rp(HttpMessageProcessor::REQUEST); // (message_processor::REQUEST);

		rp.on_request = [&](BufferRef&& method, BufferRef&& entity, int major, int minor)
		{
			CPPUNIT_ASSERT(equals(method, "GET"));
			CPPUNIT_ASSERT(equals(entity, "/"));
			CPPUNIT_ASSERT(major == 1);
			CPPUNIT_ASSERT(minor== 1);
		};

		int header_count = 0;
		rp.on_header = [&](BufferRef&& name, BufferRef&& value)
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
		rp.on_content = [&](BufferRef&& chunk)
		{
			++chunk_count;
			CPPUNIT_ASSERT(chunk_count == 1);
			CPPUNIT_ASSERT(equals(chunk, "hello world"));
			return true;
		};

		Buffer r(
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
		Buffer r(
			"HTTP/1.1 304 Not Modified\r\n"
			"Date: Mon, 19 Apr 2010 14:56:34 GMT\r\n"
			"Server: Apache\r\n"
			"Connection: close\r\n"
			"ETag: \"37210c-33b5-483 1136540000\"\r\n"
			"\r\n"
		);

		HttpMessageProcessor_component rp(HttpMessageProcessor::RESPONSE);
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
		HttpMessageProcessor_component rp(HttpMessageProcessor::RESPONSE);

		rp.on_status = [&](int vmajor, int vminor, int code, const BufferRef& text)
		{
			CPPUNIT_ASSERT(vmajor == 1);
			CPPUNIT_ASSERT(vminor == 1);
			CPPUNIT_ASSERT(code == 200);
			CPPUNIT_ASSERT(text == "Ok");
		};

		rp.on_header = [&](const BufferRef& name, const BufferRef& value)
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

		rp.on_content = [&](const BufferRef& content)
		{
			CPPUNIT_ASSERT(++body_count == 1);
			CPPUNIT_ASSERT(content == "some-body");
			return true;
		};

		Buffer r(
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
		HttpMessageProcessor_component rp(HttpMessageProcessor::RESPONSE);

		rp.on_status = [&](int vmajor, int vminor, int code, const BufferRef& text)
		{
			CPPUNIT_ASSERT(vmajor == 1);
			CPPUNIT_ASSERT(vminor == 1);
			CPPUNIT_ASSERT(code == 200);
			CPPUNIT_ASSERT(text == "");
		};

		rp.on_header = [&](const BufferRef& name, const BufferRef& value)
		{
			CPPUNIT_ASSERT(++header_count == 1);
			CPPUNIT_ASSERT(name == "Content-Length");
			CPPUNIT_ASSERT(value == "9");
		};

		rp.on_content = [&](const BufferRef& content)
		{
			CPPUNIT_ASSERT(++body_count == 1);
			CPPUNIT_ASSERT(content == "some body");
			return true;
		};

		Buffer r(
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
		HttpMessageProcessor_component rp(HttpMessageProcessor::REQUEST);

		int request_count = 0;
		rp.on_request = [&](BufferRef&& method, BufferRef&& url, int major, int minor)
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

		rp.on_header = [&](const BufferRef& name, const BufferRef& value)
		{
			CPPUNIT_ASSERT(0 == "no headers expected");
		};

		int on_header_done_invoked = 0;
		rp.on_header_done = [&]()
		{
			++on_header_done_invoked;
			return true;
		};

		rp.on_content = [&](const BufferRef& content)
		{
			CPPUNIT_ASSERT(0 == "no content expected");
			return true;
		};

		Buffer r(
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
		Buffer r(
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nsome\r\n"
			"1\r\n \r\n"
			"4\r\nbody\r\n"
			"0\r\n\r\n"
			"GARBAGE"
		);
		HttpMessageProcessor_component rp(HttpMessageProcessor::MESSAGE);

		int chunk_index = 0;
		rp.on_content = [&](const BufferRef& chunk)
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
		CPPUNIT_ASSERT(ec == HttpMessageError::aborted);
	}

	void message_chunked_body_fragmented()
	{
		Buffer r;
		r.push_back("Transfer-Encoding: chunked\r\n\r\n");
		BufferRef headers = r.ref();

		r.push_back("4\r\nsome\r\n");
		BufferRef c1(r.ref(headers.size()));

		r.push_back("1\r\n \r\n");
		BufferRef c2(r.ref(c1.offset() + c1.size()));

		r.push_back("4\r\nbody\r\n");
		BufferRef c3(r.ref(c2.offset() + c2.size()));

		r.push_back("0\r\n\r\n");
		BufferRef c4(r.ref(c3.offset() + c3.size()));

		HttpMessageProcessor_component rp(HttpMessageProcessor::MESSAGE);
		std::size_t np = 0;
		std::error_code ec;

		ec = rp.process(BufferRef(headers), np);
		CPPUNIT_ASSERT(ec == HttpMessageError::partial);

		ec = rp.process(BufferRef(c1), np);
		CPPUNIT_ASSERT(ec == HttpMessageError::partial);

		ec = rp.process(BufferRef(c2), np);
		CPPUNIT_ASSERT(ec == HttpMessageError::partial);

		ec = rp.process(BufferRef(c3), np);
		CPPUNIT_ASSERT(ec == HttpMessageError::partial);

		ec = rp.process(BufferRef(c4), np);
		CPPUNIT_ASSERT(ec == HttpMessageError::success);

		CPPUNIT_ASSERT(np == r.size());
	}

	void message_content_length()
	{
		Buffer r(
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
			"GARBAGE"
		);

		HttpMessageProcessor_component rp(HttpMessageProcessor::MESSAGE);

		rp.on_content = [&](const BufferRef& chunk)
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
		CPPUNIT_ASSERT(ec == HttpMessageError::aborted);
	}

	void message_content_recursive()
	{
		Buffer r(
			"Content-Length: 9\r\n"
			"\r\n"
			"some body"
		);

		HttpMessageProcessor_component rp(HttpMessageProcessor::MESSAGE);
		std::size_t np = 0;

		rp.on_header_done = [&]()
		{
			std::size_t npl = 0;
			std::error_code ec = rp.process(r.ref(np, r.size() - np), npl);

			CPPUNIT_ASSERT(npl == 9);
			CPPUNIT_ASSERT(!ec);

			np += npl;

			return false;
		};

		rp.on_content = [&](const BufferRef& chunk)
		{
			DEBUG("on_content('%s')", chunk.str().c_str());
			CPPUNIT_ASSERT(equals(chunk, "some body"));
			return true;
		};

		rp.on_complete = [&]()
		{
			return true;
		};

		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size());
		CPPUNIT_ASSERT(ec == HttpMessageError::aborted); // cancelled parsing at header-done state
	}

	void message_multi()
	{
		Buffer r(
			"Content-Length: 11\r\n"
			"\r\n"
			"some body\r\n"
			"Content-Length: 12\r\n"
			"\r\n"
			"some body2\r\n"
		);

		std::size_t np = 0;
		std::size_t count = 0;

		HttpMessageProcessor_component rp(HttpMessageProcessor::MESSAGE);
		rp.on_complete = [&]() { ++count; return true; };

		std::error_code ec = rp.process(r, np);

		CPPUNIT_ASSERT(np == r.size());
		CPPUNIT_ASSERT(ec == HttpMessageError::success);
		CPPUNIT_ASSERT(count == 2);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpMessageProcessor_test);
#endif
