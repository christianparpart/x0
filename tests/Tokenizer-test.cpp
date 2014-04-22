#include <gtest/gtest.h>
#include <x0/Tokenizer.h>
#include <x0/Buffer.h>

class TokenizerTest : public ::testing::Test
{
public:
    typedef x0::Tokenizer<x0::BufferRef, x0::BufferRef> BufferTokenizer;

    void SetUp();
    void TearDown();

    void Tokenize3();
};

void TokenizerTest::SetUp()
{
}

void TokenizerTest::TearDown()
{
}

TEST_F(TokenizerTest, Tokenize3)
{
    x0::Buffer input("/foo/bar/com");
    BufferTokenizer st(input.ref(1), "/");
    std::vector<x0::BufferRef> tokens = st.tokenize();

    ASSERT_EQ(3, tokens.size());
    EXPECT_EQ("foo", tokens[0]);
    EXPECT_EQ("bar", tokens[1]);
    EXPECT_EQ("com", tokens[2]);
}
