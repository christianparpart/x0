#include <x0/message_parser2.hpp>
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
		CPPUNIT_TEST(simple);
	CPPUNIT_TEST_SUITE_END();

private:
	void simple()
	{
		message_parser rp;

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
};

CPPUNIT_TEST_SUITE_REGISTRATION(message_parser_test);
#endif
