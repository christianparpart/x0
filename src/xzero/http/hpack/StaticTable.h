// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include <xzero/http/HeaderField.h>
#include <algorithm>
#include <vector>
#include <string>
#include <utility>
#include <cstdint>

namespace xzero {
namespace http {
namespace hpack {

/**
 * Static non-modifiable HeaderField Table.
 *
 * The static table (see Section 2.3.1) is a table that statically associates
 * header fields that occur frequently with index values. This table is ordered,
 * read-only, always accessible, and it may be shared amongst all encoding or
 * decoding contexts.
 *
 */
class StaticTable {
 public:
  typedef const HeaderField* iterator;

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
   * @param field the (name,value?) pair to search for.
   * @param nameValueMatch output parameter that will contain the match type
   *                       that is @c true if it was a full (name,value)-match
   *                       or @c false if just a name-match.
   *
   * @return @c 0 if not found or the index into the DynamicTable if found.
   */
  static size_t find(const HeaderField& field, bool* nameValueMatch);

  static const HeaderField& at(size_t index);

  static iterator begin();
  static iterator end();

 private:
  static HeaderField entries_[];
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
