#include <xzero/http/hpack/StaticTable.h>
#include <gtest/gtest.h>

using xzero::http::hpack::StaticTable;

TEST(hpack_StaticTable, find_field_name_only) {
  bool nameValueMatch;
  const size_t index = StaticTable::find(":path", "/custom", &nameValueMatch);
  EXPECT_EQ(3, index);
  EXPECT_FALSE(nameValueMatch);
}

TEST(hpack_StaticTable, find_field) {
  bool nameValueMatch;
  const size_t path_slash = StaticTable::find({":path", "/"}, &nameValueMatch);
  EXPECT_EQ(3, path_slash);
  EXPECT_TRUE(nameValueMatch);

  const size_t not_found = StaticTable::find({"not", "found"}, &nameValueMatch);
  EXPECT_EQ(StaticTable::npos, not_found);
}
