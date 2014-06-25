// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/Buffer.h>
#include <x0/http/HttpMessageParser.h>
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
#	warning "Compiler does not support lambda expressions. Cannot unit-test HttpMessageParser"
#else

class HttpMessageParser_component : // {{{
    public x0::HttpMessageParser
{
public:
    HttpMessageParser_component(HttpMessageParser::ParseMode mode) :
        HttpMessageParser(mode)
    {
    }

    std::function<void(const BufferRef&, const BufferRef&, int, int)> on_request;
    std::function<void(int, int, int, const BufferRef&)> on_status;
    std::function<void()> on_message;
    std::function<void(const BufferRef&, const BufferRef&)> on_header;
    std::function<bool()> on_header_done;
    std::function<bool(const BufferRef&)> on_content;
    std::function<bool()> on_complete;

private:
    virtual bool onMessageBegin(const BufferRef& method, const BufferRef& uri, int version_major, int version_minor)
    {
        if (on_request)
            on_request(method, uri, version_major, version_minor);

        return true;
    }

    virtual bool onMessageBegin(int version_major, int version_minor, int code, const BufferRef& text)
    {
        if (on_status)
            on_status(version_major, version_minor, code, text);

        return true;
    }

    virtual bool onMessageBegin()
    {
        if (on_message)
            on_message();

        return true;
    }

    virtual bool onMessageHeader(const BufferRef& name, const BufferRef& value)
    {
        if (on_header)
            on_header(name, value);

        return true;
    }

    virtual bool onMessageHeaderEnd()
    {
        if (on_header_done)
            return on_header_done();

        return true;
    }

    virtual bool onMessageContent(const BufferRef& chunk)
    {
        if (on_content)
            return on_content(chunk);

        return true;
    }

    virtual bool onMessageEnd()
    {
        if (on_complete)
            return on_complete();

        return true;
    }

    virtual void log(x0::LogMessage&& msg)
    {
    }
}; // }}}

class HttpMessageParser_test :
    public CPPUNIT_NS::TestFixture
{
public:
    CPPUNIT_TEST_SUITE(HttpMessageParser_test);
        CPPUNIT_TEST(request_simple);
        CPPUNIT_TEST(request_complex_lws_headers);
        CPPUNIT_TEST(request_no_header);

        CPPUNIT_TEST(response_simple);
        CPPUNIT_TEST(response_sample_304);
        CPPUNIT_TEST(response_no_status_text);

        CPPUNIT_TEST(message_chunked_body);
        //CPPUNIT_TEST(message_chunked_body_fragmented);
        CPPUNIT_TEST(message_content_length);
        CPPUNIT_TEST(message_multi);
    CPPUNIT_TEST_SUITE_END();

private:
    void request_complex_lws_headers()
    {
        HttpMessageParser_component rp(HttpMessageParser::REQUEST);

        Buffer r(
            "GET /foo HTTP/1.1\r\n"
            "Single-Line: single value\r\n"            // simple LWS at the beginning & middle
            "Multi-Line-1: multi\r\n\tvalue 1\r\n"     // complex LWS in the middle
            "Multi-Line-2:\r\n \t \tmulti value 2\r\n" // complex LWS at the beginning
            "\r\n"
        );
        rp.process(r.ref());
    }

    void request_simple()
    {
        HttpMessageParser_component rp(HttpMessageParser::REQUEST); // (message_processor::REQUEST);

        rp.on_request = [&](const BufferRef& method, const BufferRef& entity, int major, int minor)
        {
            CPPUNIT_ASSERT(equals(method, "GET"));
            CPPUNIT_ASSERT(equals(entity, "/"));
            CPPUNIT_ASSERT(major == 1);
            CPPUNIT_ASSERT(minor== 1);
        };

        int header_count = 0;
        rp.on_header = [&](const BufferRef& name, const BufferRef& value)
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
        rp.on_content = [&](const BufferRef& chunk) -> bool
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

        std::size_t np = rp.process(r.ref());

        CPPUNIT_ASSERT(np == r.size());
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

        HttpMessageParser_component rp(HttpMessageParser::RESPONSE);
        bool on_complete_invoked = false;

        rp.on_complete = [&]() -> bool
        {
            on_complete_invoked = true;
            return true;
        };

        std::size_t np = rp.process(r.ref());

        CPPUNIT_ASSERT(np == r.size());
        CPPUNIT_ASSERT(on_complete_invoked == true);
    }

    void response_simple()
    {
        int header_count = 0;
        int body_count = 0;
        HttpMessageParser_component rp(HttpMessageParser::RESPONSE);

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

        rp.on_content = [&](const BufferRef& content) -> bool
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

        std::size_t np = rp.process(r.ref());

        CPPUNIT_ASSERT(np == r.size());
        CPPUNIT_ASSERT(body_count == 1);
    }

    void response_no_status_text()
    {
        int header_count = 0;
        int body_count = 0;
        HttpMessageParser_component rp(HttpMessageParser::RESPONSE);

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

        rp.on_content = [&](const BufferRef& content) -> bool
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

        std::size_t np = rp.process(r.ref());

        CPPUNIT_ASSERT(np = r.size());
        CPPUNIT_ASSERT(body_count == 1);
    }

    void request_no_header()
    {
        HttpMessageParser_component rp(HttpMessageParser::REQUEST);

        int request_count = 0;
        rp.on_request = [&](const BufferRef& method, const BufferRef& url, int major, int minor)
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
        rp.on_header_done = [&]() -> bool
        {
            ++on_header_done_invoked;
            return true;
        };

        rp.on_content = [&](const BufferRef& content) -> bool
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

        std::size_t np = rp.process(r.ref());

        CPPUNIT_ASSERT(np == r.size());
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
        HttpMessageParser_component rp(HttpMessageParser::MESSAGE);

        int chunk_index = 0;
        rp.on_content = [&](const BufferRef& chunk) -> bool
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
        rp.on_complete = [&]() -> bool
        {
            return false;
        };

        std::size_t np = rp.process(r.ref());

        CPPUNIT_ASSERT(np == r.size() - 7);
        CPPUNIT_ASSERT(rp.state() == x0::HttpMessageParser::SYNTAX_ERROR);
    }

    void message_chunked_body_fragmented()
    {
#if 0
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

        HttpMessageParser_component rp(HttpMessageParser::MESSAGE);
        std::size_t np = 0;
        std::error_code ec;

        ec = rp.process(BufferRef(headers), np);
        CPPUNIT_ASSERT(ec == HttpMessageError::Partial);

        ec = rp.process(BufferRef(c1), np);
        CPPUNIT_ASSERT(ec == HttpMessageError::Partial);

        ec = rp.process(BufferRef(c2), np);
        CPPUNIT_ASSERT(ec == HttpMessageError::Partial);

        ec = rp.process(BufferRef(c3), np);
        CPPUNIT_ASSERT(ec == HttpMessageError::Partial);

        ec = rp.process(BufferRef(c4), np);
        CPPUNIT_ASSERT(ec == HttpMessageError::Success);

        CPPUNIT_ASSERT(np == r.size());
#endif
    }

    void message_content_length()
    {
        Buffer r(
            "Content-Length: 9\r\n"
            "\r\n"
            "some body"
            "GARBAGE"
        );

        HttpMessageParser_component rp(HttpMessageParser::MESSAGE);

        rp.on_content = [&](const BufferRef& chunk) -> bool
        {
            CPPUNIT_ASSERT(equals(chunk, "some body"));
            return true;
        };
        rp.on_complete = [&]() -> bool
        {
            return false;
        };

        std::size_t np = rp.process(r.ref());

        CPPUNIT_ASSERT(np == r.size() - 7);
        CPPUNIT_ASSERT(rp.state() == x0::HttpMessageParser::SYNTAX_ERROR);
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

        std::size_t count = 0;

        HttpMessageParser_component rp(HttpMessageParser::MESSAGE);
        rp.on_complete = [&]() -> bool { ++count; return true; };

        std::size_t np =  rp.process(r.ref());

        CPPUNIT_ASSERT(np == r.size());
        CPPUNIT_ASSERT(count == 2);
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpMessageParser_test);
#endif
