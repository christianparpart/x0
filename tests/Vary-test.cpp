#include <gtest/gtest.h>
#include <x0/http/Vary.h>
#include <vector>

using namespace x0;

typedef HttpHeader<BufferRef> Header;

void dump(const Vary& vary)
{
	printf("Vary fields (%zu):\n", vary.size());
	for (size_t i = 0, e = vary.size(); i != e; ++i) {
		printf("%20s: %s\n", vary.names()[i].str().c_str(),
				vary.values()[i].str().c_str());
	}
}

auto requestHeaders() -> const std::vector<Header>& {
	static const std::vector<Header> headers = {
		{ "Accept-Encoding", "gzip" },
		{ "X-Test", "42" },
		{ "User-Agent", "gtest" },
	};
	return headers;
}

TEST(Vary, Create0)
{
	auto vary = Vary::create("", requestHeaders());
	ASSERT_TRUE(vary.get() != nullptr);
	ASSERT_EQ(0, vary->size());

	auto i = vary->begin();
	auto e = vary->end();
	ASSERT_EQ(true, i == e);
}

TEST(Vary, Create1)
{
	auto vary = Vary::create("Accept-Encoding", requestHeaders());

	ASSERT_EQ(1, vary->size());
	ASSERT_EQ("Accept-Encoding", vary->names()[0]);
	ASSERT_EQ("gzip", vary->values()[0]);
}

TEST(Vary, Create2)
{
	auto vary = Vary::create("Accept-Encoding,User-Agent", requestHeaders());

	ASSERT_EQ(2, vary->size());

	ASSERT_EQ("Accept-Encoding", vary->names()[0]);
	ASSERT_EQ("gzip", vary->values()[0]);

	ASSERT_EQ("User-Agent", vary->names()[1]);
	ASSERT_EQ("gtest", vary->values()[1]);
}

TEST(Vary, Foreach0)
{
	auto vary = Vary::create("", requestHeaders());
	ASSERT_TRUE(vary.get() != nullptr);
	auto i = vary->begin();
	auto e = vary->end();
	ASSERT_EQ(true, i == e);
}

TEST(Vary, Foreach1)
{
	auto vary = Vary::create("Accept-Encoding", requestHeaders());

	auto i = vary->begin();
	auto e = vary->end();

	ASSERT_TRUE(i != e);
	ASSERT_EQ("Accept-Encoding", i.name());
	ASSERT_EQ("gzip", i.value());

	++i;
	ASSERT_TRUE(i == e);
}

TEST(Vary, Foreach2)
{
	auto vary = Vary::create("Accept-Encoding,User-Agent", requestHeaders());

	auto i = vary->begin();
	auto e = vary->end();
	ASSERT_EQ("Accept-Encoding", i.name());
	ASSERT_EQ("gzip", i.value());

	++i;
	ASSERT_EQ("User-Agent", i.name());
	ASSERT_EQ("gtest", i.value());

	++i;
	ASSERT_EQ(true, i == e);
}
