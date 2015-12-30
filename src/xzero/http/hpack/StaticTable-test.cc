#include <xzero/http/hpack/StaticTable.h>
#include <gtest/gtest.h>

using xzero::http::hpack::StaticTable;

TEST(hpack_StaticTable, find_field_name_only) {
  size_t index;
  bool nameValueMatch;

  bool match = StaticTable::find(":path", "/custom", &index, &nameValueMatch);

  EXPECT_TRUE(match);
  EXPECT_EQ(3, index);
  EXPECT_FALSE(nameValueMatch);
}

TEST(hpack_StaticTable, find_field_fully) {
  size_t index;
  bool nameValueMatch;

  bool match = StaticTable::find(":path", "/", &index, &nameValueMatch);

  EXPECT_TRUE(match);
  EXPECT_EQ(3, index);
  EXPECT_TRUE(nameValueMatch);
}

TEST(hpack_StaticTable, find_field_nothing) {
  size_t index;
  bool nameValueMatch;

  bool match = StaticTable::find("not", "found", &index, &nameValueMatch);
  EXPECT_FALSE(match);
}

TEST(hpack_StaticTable, find_them_all_binary_search_test) {
  // make sure we find them all, just to unit-test our binary search
  size_t index;
  bool nameValueMatch;

  for (size_t i = 0; i < StaticTable::length(); i++) {
    bool match = StaticTable::find(StaticTable::at(i), &index, &nameValueMatch);
    ASSERT_EQ(index, i);
    ASSERT_EQ(true, nameValueMatch);
    ASSERT_EQ(true, match);
  }
}
