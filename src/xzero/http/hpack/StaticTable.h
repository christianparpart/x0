// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include <xzero/http/hpack/TableEntry.h>
#include <algorithm>
#include <vector>
#include <string>
#include <utility>
#include <cstdint>

namespace xzero {
namespace http {
namespace hpack {

/**
 * Static non-modifiable Header Field Table.
 *
 * The static table (see Section 2.3.1) is a table that statically associates
 * header fields that occur frequently with index values. This table is ordered,
 * read-only, always accessible, and it may be shared amongst all encoding or
 * decoding contexts.
 *
 */
class StaticTable {
 public:
  typedef const TableEntry* iterator;

  /**
   * Represents the "not found" case when searching for elements in a table.
   */
  enum { npos = static_cast<size_t>(-1) };

  /**
   * Retrieves the total number of fields within the StaticTable.
   */
  static size_t length();

  /**
   * Retrieves the index of the given header @p field or @c npos if not found.
   *
   * @param entry Header field name/value pair to match for.
   * @param index output parameter that will contain the matching table index
   * @param nameValueMatch output parameter that will contain the match type
   *                       that is @c true if it was a full (name,value)-match
   *                       or @c false if just a name-match.
   *
   * @retval true table entry found. @p index and @p nameValueMatch updated.
   * @retval false No table entry found.
   */
  static bool find(const TableEntry& entry,
                   size_t* index,
                   bool* nameValueMatch);

  /**
   * Retrieves the index of the given header @p field or @c npos if not found.
   *
   * @param name Header field name to match for.
   * @param value Header field value to match for (optionally).
   * @param index output parameter that will contain the matching table index
   * @param nameValueMatch output parameter that will contain the match type
   *                       that is @c true if it was a full (name,value)-match
   *                       or @c false if just a name-match.
   *
   * @retval true table entry found. @p index and @p nameValueMatch updated.
   * @retval false No table entry found.
   */
  static bool find(const std::string& name,
                   const std::string& value,
                   size_t* index,
                   bool* nameValueMatch);

  static const TableEntry& at(size_t index);

  static iterator begin();
  static iterator end();

 private:
  static TableEntry entries_[];
};

// {{{ inlines
inline StaticTable::iterator StaticTable::begin() {
  return &entries_[0];
}

inline StaticTable::iterator StaticTable::end() {
  return &entries_[length()];
}
// }}}

}  // namespace hpack
}  // namespace http
}  // namespace xzero
