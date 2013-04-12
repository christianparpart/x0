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

	shaper->createNode("vip", 0.1, 0.3);
	vip = shaper->findNode("vip");

	shaper->createNode("main", 0.5, 0.7);
	main = shaper->findNode("main");

	main->createChild("upload", 0.5, 0.5);
	upload = shaper->findNode("upload");
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
	ASSERT_TRUE(vip != nullptr);
	ASSERT_TRUE(main != nullptr);
	ASSERT_TRUE(upload != nullptr);

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

TEST_F(TokenShaperTest, CreateErrors)
{
	ASSERT_EQ(TokenShaperError::RateLimitOverflow, shaper->createNode("special", 0.41));
	ASSERT_EQ(TokenShaperError::RateLimitOverflow, shaper->createNode("special", 1.1));
	ASSERT_EQ(TokenShaperError::RateLimitOverflow, shaper->createNode("special", -0.1));
	ASSERT_EQ(TokenShaperError::CeilLimitOverflow, vip->createChild("special", 1.0, 0.40));
	ASSERT_EQ(TokenShaperError::CeilLimitOverflow, vip->createChild("special", 1.0, 1.01));

	ASSERT_EQ(TokenShaperError::NameConflict, vip->createChild("vip", 1.0, 1.0));
}

TEST_F(TokenShaperTest, MutateErrors)
{
	EXPECT_EQ(TokenShaperError::NameConflict, vip->setName("main"));

	EXPECT_EQ(TokenShaperError::RateLimitOverflow, vip->setRate(0.4));
	EXPECT_EQ(TokenShaperError::RateLimitOverflow, vip->setRate(-0.1));
	EXPECT_EQ(TokenShaperError::RateLimitOverflow, vip->setRate(1.1));

	EXPECT_EQ(TokenShaperError::CeilLimitOverflow, vip->setCeil(0.09));
	EXPECT_EQ(TokenShaperError::CeilLimitOverflow, vip->setCeil(1.1));
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

TEST_F(TokenShaperTest, SetRate)
{
	// increase rate from 0.5 to 0.6;
	// this should also update the token rates from this node and all its child nodes recursively.
	main->setRate(0.6f);

	ASSERT_EQ(0.6f, main->rate());

	ASSERT_EQ(6, main->tokenRate());
	ASSERT_EQ(7, main->tokenCeil());

	ASSERT_EQ(3, upload->tokenRate());
	ASSERT_EQ(3, upload->tokenCeil());
}

TEST_F(TokenShaperTest, SetCeil)
{
	// increase rate from 0.5 to 0.6;
	// this should also update the token rates from this node and all its child nodes recursively.
	main->setCeil(0.8);

	ASSERT_EQ(0.8f, main->ceil());
	ASSERT_EQ(5, main->tokenRate());
	ASSERT_EQ(8, main->tokenCeil());

	ASSERT_EQ(2, upload->tokenRate());
	ASSERT_EQ(4, upload->tokenCeil());
}

TEST_F(TokenShaperTest, GetWithEnqueuePutDequeue)
{
	ASSERT_EQ(1, vip->get()); // passes through
	ASSERT_EQ(1, vip->get()); // passes through
	ASSERT_EQ(1, vip->get()); // passes through (overrate)
	ASSERT_EQ(0, vip->get()); // ok, we must enqueue it then

	vip->enqueue(new int(42));
	ASSERT_EQ(1, vip->queued().current());

	vip->enqueue(new int(43));
	ASSERT_EQ(2, vip->queued().current());
	EXPECT_EQ(0, vip->tokensAvailable());

	// attempt to dequeue when we shouldn't be able to, because we have no spare tokens.
	auto object = root->dequeue();
	ASSERT_TRUE(object == nullptr);

	vip->put();
	EXPECT_EQ(1, vip->tokensAvailable());

	object = root->dequeue();
	ASSERT_TRUE(object != nullptr);
	EXPECT_EQ(42, *object);
	ASSERT_EQ(1, vip->queued().current());
	EXPECT_EQ(0, vip->tokensAvailable());
	delete object;

	// Another dequeue must fail because we have no tokens available on vip node.
	object = root->dequeue();
	ASSERT_TRUE(object == nullptr);

	// Free a token up on vip node.
	vip->put();
	ASSERT_EQ(1, vip->tokensAvailable());

	// Actually dequeue the last item.
	object = root->dequeue();
	ASSERT_TRUE(object != nullptr);
	EXPECT_EQ(43, *object);
	ASSERT_EQ(0, vip->queued().current());
	delete object;

	// Release the 2 remaining tokens.
	vip->put();
	vip->put();

	// Another dequeue should fail because we have nothing to dequeue anymore.
	object = root->dequeue();
	ASSERT_TRUE(object == nullptr);
}

TEST_F(TokenShaperTest, TimeoutHandling)
{
	ev::tstamp start_at = ev::now(shaper->loop());
	ev::tstamp fired_at = 0;
	int* object = nullptr;

	vip->setQueueTimeout(TimeSpan::fromSeconds(1));
	vip->setTimeoutHandler([&](int* obj) {
		fired_at = ev::now(shaper->loop());
		object = obj;
		shaper->loop().break_loop();
	});

	vip->enqueue(new int(42));
	shaper->loop().run();

	ev::tstamp duration = fired_at - start_at;
	ev::tstamp diff = vip->queueTimeout()() - duration;

	EXPECT_TRUE(diff >= -0.001);
	EXPECT_TRUE(diff <= +0.001);
	ASSERT_TRUE(object != nullptr);
	EXPECT_EQ(42, *object);

	delete object;
}

// TODO: test timeout handling
