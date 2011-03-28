#include <x0/http/HttpRangeDef.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
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
		CPPUNIT_TEST(range2);
		CPPUNIT_TEST(range3);
		CPPUNIT_TEST(range4);
		CPPUNIT_TEST(range5);
		CPPUNIT_TEST(range6);
	CPPUNIT_TEST_SUITE_END();

private:
	void range1()
	{
		x0::HttpRangeDef r;
		x0::ConstBuffer spec("bytes=0-499");

		r.parse(spec);
		CPPUNIT_ASSERT(r.unitName() == "bytes");
		CPPUNIT_ASSERT(r.size() == 1);
		CPPUNIT_ASSERT(r[0].first == 0);
		CPPUNIT_ASSERT(r[0].second == 499);
	}

	void range2()
	{
		x0::HttpRangeDef r;
		x0::ConstBuffer spec("bytes=500-999");

		r.parse(spec);
		CPPUNIT_ASSERT(r.unitName() == "bytes");
		CPPUNIT_ASSERT(r.size() == 1);
		CPPUNIT_ASSERT(r[0].first == 500);
		CPPUNIT_ASSERT(r[0].second == 999);
	}

	void range3()
	{
		x0::HttpRangeDef r;

		x0::ConstBuffer spec("bytes=-500");
		r.parse(spec);
		CPPUNIT_ASSERT(r.unitName() == "bytes");
		CPPUNIT_ASSERT(r.size() == 1);
		CPPUNIT_ASSERT(r[0].first == x0::HttpRangeDef::npos);
		CPPUNIT_ASSERT(r[0].second == 500);
	}

	void range4()
	{
		x0::HttpRangeDef r;

		x0::ConstBuffer spec("bytes=9500-");
		r.parse(spec);
		CPPUNIT_ASSERT(r.unitName() == "bytes");
		CPPUNIT_ASSERT(r.size() == 1);
		CPPUNIT_ASSERT(r[0].first == 9500);
		CPPUNIT_ASSERT(r[0].second == x0::HttpRangeDef::npos);
	}

	void range5()
	{
		x0::HttpRangeDef r;

		x0::ConstBuffer spec("bytes=0-0,-1");
		r.parse(spec);
		CPPUNIT_ASSERT(r.unitName() == "bytes");
		CPPUNIT_ASSERT(r.size() == 2);

		CPPUNIT_ASSERT(r[0].first == 0);
		CPPUNIT_ASSERT(r[0].second == 0);

		CPPUNIT_ASSERT(r[1].first == x0::HttpRangeDef::npos);
		CPPUNIT_ASSERT(r[1].second == 1);
	}

	void range6()
	{
		x0::HttpRangeDef r;

		x0::ConstBuffer spec("bytes=500-700,601-999");
		r.parse(spec);
		CPPUNIT_ASSERT(r.unitName() == "bytes");
		CPPUNIT_ASSERT(r.size() == 2);

		CPPUNIT_ASSERT(r[0].first == 500);
		CPPUNIT_ASSERT(r[0].second == 700);

		CPPUNIT_ASSERT(r[1].first == 601);
		CPPUNIT_ASSERT(r[1].second == 999);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(range_def_test);
