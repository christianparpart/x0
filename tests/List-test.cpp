#include <gtest/gtest.h>
#include <x0/List.h>
#include <vector>

using namespace x0;

TEST(ListTest, Empty)
{
	List<int> r;
	ASSERT_TRUE(r.empty());
	ASSERT_FALSE(!r.empty());
	ASSERT_EQ(0, r.size());
}

TEST(ListTest, Add)
{
	List<int> r;
	r.push_front(7);
	ASSERT_EQ(1, r.size());
	ASSERT_EQ(7, r.front());

	r.push_front(9);
	ASSERT_EQ(2, r.size());
	ASSERT_EQ(9, r.front());
}

TEST(ListTest, Remove)
{
	List<int> r;
	r.push_front(1);
	r.push_front(2);
	r.push_front(3);

	ASSERT_FALSE(r.remove(42));
	ASSERT_TRUE(r.remove(2));
	ASSERT_EQ(2, r.size());
}

TEST(ListTest, Each)
{
	List<int> r;
	r.push_front(1);
	r.push_front(2);
	r.push_front(3);

	std::vector<int> v;

	r.each([&](int& value) -> bool {
		v.push_back(value);
		return true;
	});

	ASSERT_EQ(3, v.size());
	ASSERT_EQ(3, v[0]);
	ASSERT_EQ(2, v[1]);
	ASSERT_EQ(1, v[2]);
}
