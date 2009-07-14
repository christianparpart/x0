#include <x0/range_def.hpp>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

class range_def_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(range_def_test);
		CPPUNIT_TEST(range1);
	CPPUNIT_TEST_SUITE_END();

private:
	void range1()
	{
		x0::range_def r;

		r.parse("bytes=1-3");
		CPPUNIT_ASSERT(r.unit_name() == "bytes");
		CPPUNIT_ASSERT(r.size() == 1);
		CPPUNIT_ASSERT(r[0].first == 1);
		CPPUNIT_ASSERT(r[0].second == 3);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(range_def_test);













