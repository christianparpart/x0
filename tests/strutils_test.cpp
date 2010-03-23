#include <x0/strutils.hpp>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

class strutils_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(strutils_test);
		CPPUNIT_TEST(parse_url_full);
		CPPUNIT_TEST(parse_url_0);
		CPPUNIT_TEST(parse_url_1);
		CPPUNIT_TEST(parse_url_2);
		CPPUNIT_TEST(parse_url_3);
		CPPUNIT_TEST(parse_url_4);
	CPPUNIT_TEST_SUITE_END();

private:
	void parse_url_full()
	{
		std::string protocol, hostname, path, query;
		int port = -1;

		bool rv = x0::parse_url("http://xzero.ws:8080/path/to?query", protocol, hostname, port, path, query);

		CPPUNIT_ASSERT(rv == true);
		CPPUNIT_ASSERT(protocol == "http");
		CPPUNIT_ASSERT(hostname == "xzero.ws");
		CPPUNIT_ASSERT(port == 8080);
		CPPUNIT_ASSERT(path == "/path/to");
		CPPUNIT_ASSERT(query == "query");
	}

	void parse_url_0()
	{
		std::string protocol, hostname, path, query;
		int port = -1;

		bool rv = x0::parse_url("http://xzero.ws", protocol, hostname, port, path, query);

		CPPUNIT_ASSERT(rv == true);
		CPPUNIT_ASSERT(protocol == "http");
		CPPUNIT_ASSERT(hostname == "xzero.ws");
		CPPUNIT_ASSERT(port == 80);
		CPPUNIT_ASSERT(path == "");
		CPPUNIT_ASSERT(query == "");
	}

	void parse_url_1()
	{
		std::string protocol, hostname, path, query;
		int port = -1;

		bool rv = x0::parse_url("http://xzero.ws/", protocol, hostname, port, path, query);

		CPPUNIT_ASSERT(rv == true);
		CPPUNIT_ASSERT(protocol == "http");
		CPPUNIT_ASSERT(hostname == "xzero.ws");
		CPPUNIT_ASSERT(port == 80);
		CPPUNIT_ASSERT(path == "/");
		CPPUNIT_ASSERT(query == "");
	}

	void parse_url_2()
	{
		std::string protocol, hostname, path, query;
		int port = -1;

		bool rv = x0::parse_url("https://xzero.ws/", protocol, hostname, port, path, query);

		CPPUNIT_ASSERT(rv == true);
		CPPUNIT_ASSERT(protocol == "https");
		CPPUNIT_ASSERT(hostname == "xzero.ws");
		CPPUNIT_ASSERT(port == 443);
		CPPUNIT_ASSERT(path == "/");
		CPPUNIT_ASSERT(query == "");
	}

	void parse_url_3()
	{
		std::string protocol, hostname, path, query;
		int port = -1;

		bool rv = x0::parse_url("http://xzero.ws/?query", protocol, hostname, port, path, query);

		CPPUNIT_ASSERT(rv == true);
		CPPUNIT_ASSERT(protocol == "http");
		CPPUNIT_ASSERT(hostname == "xzero.ws");
		CPPUNIT_ASSERT(port == 80);
		CPPUNIT_ASSERT(path == "/");
		CPPUNIT_ASSERT(query == "query");
	}

	void parse_url_4()
	{
		std::string protocol, hostname, path, query;
		int port = -1;

		bool rv = x0::parse_url("http://::1:8080/", protocol, hostname, port, path, query);

		CPPUNIT_ASSERT(rv == true);
		CPPUNIT_ASSERT(protocol == "http");
		CPPUNIT_ASSERT(hostname == "::1");
		CPPUNIT_ASSERT(port == 8080);
		CPPUNIT_ASSERT(path == "/");
		CPPUNIT_ASSERT(query == "");
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(strutils_test);
