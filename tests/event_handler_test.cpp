#include <x0/EventHandler.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <iostream>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include <strings.h>

class event_handler_test :
	public CPPUNIT_NS::TestFixture
{
public:
	CPPUNIT_TEST_SUITE(event_handler_test);
		CPPUNIT_TEST(ctor0);
		CPPUNIT_TEST(asyncness);
		CPPUNIT_TEST(connection);
		CPPUNIT_TEST(completion_handler);
	CPPUNIT_TEST_SUITE_END();

private:
	void deadend(x0::event_handler<void(int *)>::invokation_iterator done, int *i)
	{
		*i |= 0x0001;

		// do not invoke done();
	}

	void f(const std::function<void()>& done, int *i)
	{
		*i |= 0x0002;

		done();
	}

	void inc(const std::function<void()>& done, int *i)
	{
		++*i;

		done();
	}

	void done(int *i)
	{
		++*i;
	}

private:
	void ctor0()
	{
		x0::event_handler<void()> eh;

		CPPUNIT_ASSERT(eh.empty());
		CPPUNIT_ASSERT(eh.size() == 0);
	}

	void connection()
	{
		using namespace std::placeholders;

		x0::event_handler<void(int *)> eh;

		eh.connect(std::bind(&event_handler_test::inc, this, _1, _2));
		CPPUNIT_ASSERT(eh.size() == 0);

		{
			auto c1 = eh.connect(std::bind(&event_handler_test::inc, this, _1, _2));
			CPPUNIT_ASSERT(eh.size() == 1);
		}
		CPPUNIT_ASSERT(eh.size() == 0);

		eh.connect(std::bind(&event_handler_test::inc, this, _1, _2)).detach();
		CPPUNIT_ASSERT(eh.size() == 1);
	}

	void completion_handler()
	{
		using namespace std::placeholders;

		x0::event_handler<void(int *)> eh;
		auto c1 = eh.connect(std::bind(&event_handler_test::inc, this, _1, _2));
		int i = 0;

		// no completion handler
		eh(&i);
		CPPUNIT_ASSERT(i == 1);

		// via std::bind()
		eh(std::bind(&event_handler_test::done, this, &i), &i);
		CPPUNIT_ASSERT(i == 3);

#if CC_SUPPORTS_LAMBDA
		auto lambda = [&]() {
			CPPUNIT_ASSERT(i == 4);
			++i;
		};
		eh(lambda, &i);
		CPPUNIT_ASSERT(i == 5);
#endif
	}

	void asyncness()
	{
		using namespace std::placeholders;

		x0::event_handler<void(int *)> eh;
		auto c1 = eh.connect(std::bind(&event_handler_test::deadend, this, _1, _2));
		auto c2 = eh.connect(std::bind(&event_handler_test::f, this, _1, _2));

		int i = 0;
		eh(&i);
		CPPUNIT_ASSERT(i & 0x0001);
		CPPUNIT_ASSERT(!(i & 0x0002));
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(event_handler_test);
