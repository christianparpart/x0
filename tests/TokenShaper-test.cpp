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

class TokenShaperTest : public ::testing::Test
{
public:
	TokenShaperTest();
	void SetUp();
	void TearDown();
	void dump();

protected:
	typedef TokenShaper<int> Shaper;

	Shaper* shaper;
	Shaper::Node* root;
	Shaper::Node* vip;
	Shaper::Node* main;
	Shaper::Node* upload;
};

TokenShaperTest::TokenShaperTest() :
	shaper(nullptr),
	root(nullptr),
	vip(nullptr),
	main(nullptr),
	upload(nullptr)
{
}

void TokenShaperTest::SetUp()
{
	shaper = new Shaper(ev_default_loop(0), 10);
	root = shaper->rootNode();
	vip = shaper->createNode("vip", 0.1, 0.3);
	main = shaper->createNode("main", 0.5, 0.7);
	upload = main->createChild("upload", 0.5, 0.5);
}

void TokenShaperTest::TearDown()
{
	delete shaper;
}

void TokenShaperTest::dump()
{
	::dump(*shaper, "shaper");
}

TEST_F(TokenShaperTest, Setup)
{
	ASSERT_EQ(0.1f, vip->rate());
	ASSERT_EQ(0.3f, vip->ceil());
	ASSERT_EQ(1, vip->tokenRate());
	ASSERT_EQ(3, vip->tokenCeil());

	ASSERT_EQ(0.5f, main->rate());
	ASSERT_EQ(0.7f, main->ceil());
	ASSERT_EQ(5, main->tokenRate());
	ASSERT_EQ(7, main->tokenCeil());

	ASSERT_EQ(0.5f, upload->rate());
	ASSERT_EQ(0.5f, upload->ceil());
	ASSERT_EQ(2, upload->tokenRate());
	ASSERT_EQ(3, upload->tokenCeil());
}

TEST_F(TokenShaperTest, GetPut)
{
	ASSERT_EQ(1, vip->get());

	ASSERT_EQ(1, vip->actualTokenRate());
	ASSERT_EQ(1, root->actualTokenRate());

	vip->put();
	ASSERT_EQ(0, vip->actualTokenRate());
	ASSERT_EQ(0, root->actualTokenRate());
}

TEST_F(TokenShaperTest, GetOverrate)
{
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

	// The second one gets through, too.
	ASSERT_EQ(1, vip->get());
	ASSERT_EQ(3, vip->actualTokenRate());
	ASSERT_EQ(2, vip->tokenOverRate());
	ASSERT_EQ(3, root->actualTokenRate());
	ASSERT_EQ(0, root->tokenOverRate());

	// next get() should fail, because we reached ceil already
	ASSERT_EQ(0, vip->get());

	// put the one that overrated back, and we should be back at capped rate.
	vip->put();
	ASSERT_EQ(2, vip->actualTokenRate());
	ASSERT_EQ(1, vip->tokenOverRate());
	ASSERT_EQ(2, root->actualTokenRate());
	ASSERT_EQ(0, root->tokenOverRate());

	// put the one that overrated back, and we should be back at capped rate.
	vip->put();
	ASSERT_EQ(1, vip->actualTokenRate());
	ASSERT_EQ(0, vip->tokenOverRate());
	ASSERT_EQ(1, root->actualTokenRate());
	ASSERT_EQ(0, root->tokenOverRate());
}

TEST_F(TokenShaperTest, Resize)
{
	shaper->resize(100);
	ASSERT_EQ(0.1f, vip->rate());
	ASSERT_EQ(0.3f, vip->ceil());
	ASSERT_EQ(10, vip->tokenRate());
	ASSERT_EQ(30, vip->tokenCeil());

	ASSERT_EQ(0.5f, main->rate());
	ASSERT_EQ(0.7f, main->ceil());
	ASSERT_EQ(50, main->tokenRate());
	ASSERT_EQ(70, main->tokenCeil());

	ASSERT_EQ(0.5f, upload->rate());
	ASSERT_EQ(0.5f, upload->ceil());
	ASSERT_EQ(25, upload->tokenRate());
	ASSERT_EQ(35, upload->tokenCeil());

	shaper->resize(200);
	ASSERT_EQ(0.1f, vip->rate());
	ASSERT_EQ(0.3f, vip->ceil());
	ASSERT_EQ(20, vip->tokenRate());
	ASSERT_EQ(60, vip->tokenCeil());

	ASSERT_EQ(0.5f, main->rate());
	ASSERT_EQ(0.7f, main->ceil());
	ASSERT_EQ(100, main->tokenRate());
	ASSERT_EQ(140, main->tokenCeil());

	ASSERT_EQ(0.5f, upload->rate());
	ASSERT_EQ(0.5f, upload->ceil());
	ASSERT_EQ(50, upload->tokenRate());
	ASSERT_EQ(70, upload->tokenCeil());

}
