// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/Buffer.h>
#include <x0/Base64.h>
#include <cstdio>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

class Base64Test :
    public CPPUNIT_NS::TestFixture
{
public:
    CPPUNIT_TEST_SUITE(Base64Test);
        CPPUNIT_TEST(test_decode);
    CPPUNIT_TEST_SUITE_END();

private:
    void test_decode()
    {
        CPPUNIT_ASSERT(testDecode("a",    "YQ=="));
        CPPUNIT_ASSERT(testDecode("ab",   "YWI="));
        CPPUNIT_ASSERT(testDecode("abc",  "YWJj"));
        CPPUNIT_ASSERT(testDecode("abcd", "YWJjZA=="));
        CPPUNIT_ASSERT(testDecode("foo:bar", "Zm9vOmJhcg=="));
    }

    bool testDecode(const x0::Buffer& decoded, const x0::Buffer& encoded) {
        x0::Buffer result(x0::Base64::decode(encoded.ref()));
        return x0::equals(result, decoded);
    }

private: // {{{ debug helper
    void print(const x0::Buffer& b, const char *msg = 0)
    {
        if (msg && *msg)
            printf("\nbuffer(%s): '%s'\n", msg, b.str().c_str());
        else
            printf("\nbuffer: '%s'\n", b.str().c_str());
    }

    void print(const x0::BufferRef& v, const char *msg = 0)
    {
        char prefix[64];
        if (msg && *msg)
            snprintf(prefix, sizeof(prefix), "\nbuffer.view(%s)", msg);
        else
            snprintf(prefix, sizeof(prefix), "\nbuffer.view");

        if (v)
            printf("\n%s: '%s' (size=%ld)\n", prefix, v.str().c_str(), v.size());
        else
            printf("\n%s: NULL\n", prefix);
    }
    //}}}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Base64Test);
