#include <gtest/gtest.h>
#include <x0/TokenShaper.h>
#include <x0/AnsiColor.h>
#include <ev++.h>

using namespace x0;

// {{{
template<typename T>
void dumpNode(const T* bucket, const char* title, int depth)
{
	if (title && *title)
		printf("%20s: ", title);
	else
		printf("%20s  ", "");

	for (int i = 0; i < depth; ++i)
		printf(" -- ");

	printf("name:%-20s rate:%-2zu (%.2f) ceil:%-2zu (%.2f) \tactual-rate:%-2zu queued:%-2zu\n",
			AnsiColor::colorize(AnsiColor::Green, bucket->name()).c_str(),
			bucket->tokenRate(), bucket->rate(), bucket->tokenCeil(), bucket->ceil(),
			bucket->actualTokenRate(),
			bucket->queued().current());

	for (const auto& child: *bucket)
		dumpNode(child, "", depth + 1);

	if (!depth) {
		printf("\n");
	}
}

template<class T>
void dump(const TokenShaper<T>& shaper, const char* title)
{
	dumpNode(shaper.rootNode(), title, 0);
}
// }}}

TEST(TokenShaperTest, Setup)
{
	TokenShaper<int> shaper(ev_default_loop(0), 100);

	auto vip = shaper.createNode("vip", 0.2, 1.0);
	auto main = shaper.createNode("main", 0.5, 0.7);
	auto upload = main->createChild("upload", 0.5, 0.5);

	EXPECT_EQ(0.2f, vip->rate());
	EXPECT_EQ(1.0f, vip->ceil());
	EXPECT_EQ(20, vip->tokenRate());
	EXPECT_EQ(100, vip->tokenCeil());

	EXPECT_EQ(0.5f, main->rate());
	EXPECT_EQ(0.7f, main->ceil());
	EXPECT_EQ(50, main->tokenRate());
	EXPECT_EQ(70, main->tokenCeil());

	EXPECT_EQ(0.5f, upload->rate());
	EXPECT_EQ(0.5f, upload->ceil());
	EXPECT_EQ(25, upload->tokenRate());
	EXPECT_EQ(35, upload->tokenCeil());
}

TEST(TokenShaperTest, GetPut)
{
	TokenShaper<int> shaper(ev_default_loop(0), 10);

	auto root = shaper.rootNode();
	auto vip = shaper.createNode("vip", 0.2, 1.0);

	ASSERT_EQ(1, vip->get());

	ASSERT_EQ(1, vip->actualTokenRate());
	ASSERT_EQ(1, root->actualTokenRate());

	vip->put();
	ASSERT_EQ(0, vip->actualTokenRate());
	ASSERT_EQ(0, root->actualTokenRate());
}

TEST(TokenShaperTest, GetOverrate)
{
	TokenShaper<int> shaper(ev_default_loop(0), 10);

	auto root = shaper.rootNode();
	auto vip = shaper.createNode("vip", 0.1, 0.2);

	ASSERT_EQ(1, vip->get());

	ASSERT_EQ(1, vip->actualTokenRate());
	ASSERT_EQ(0, vip->tokenOverRate());
	ASSERT_EQ(1, root->actualTokenRate());
	ASSERT_EQ(0, root->tokenOverRate());

	// now get() one that must be enqueued
	ASSERT_EQ(1, vip->get());
	ASSERT_EQ(2, vip->actualTokenRate());
	ASSERT_EQ(1, vip->tokenOverRate());
	ASSERT_EQ(2, root->actualTokenRate());
	ASSERT_EQ(0, root->tokenOverRate());

	// next get() should fail, because we reached ceil already
	ASSERT_EQ(0, vip->get());

	// put the one that overrated back, and we should be back at capped rate.
	vip->put();
	ASSERT_EQ(1, vip->actualTokenRate());
	ASSERT_EQ(0, vip->tokenOverRate());
	ASSERT_EQ(1, root->actualTokenRate());
	ASSERT_EQ(0, root->tokenOverRate());
}
