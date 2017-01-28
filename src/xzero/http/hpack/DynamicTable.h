// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/hpack/TableEntry.h>
#include <deque>
#include <string>
#include <utility>
#include <cstdint>

namespace xzero {
namespace http {
namespace hpack {

/**
  * Compression Sensitive Header Field Table.
  *
  * The dynamic table (see Section 2.3.2) is a table that associates stored
  * header fields with index values. This table is dynamic and specific to an
  * encoding or decoding context.
  */
class DynamicTable {
 public:
  explicit DynamicTable(size_t maxSize);

  /**
   * Represents the "not found" case when searching for elements in a table.
   */
  enum { npos = static_cast<size_t>(-1) };

  // XXX: The additional 32 octets account for an estimated overhead associated
  // with an entry. For example, an entry structure using two 64-bit pointers to
  // reference the name and the value of the entry and two 64-bit integers for
  // counting the number of references to the name and value would have 32
  // octets of overhead.
  enum { HeaderFieldOverheadSize = 32 };

  /**
   * Retrieves number of fields in the table.
   */
  size_t length() const noexcept { return entries_.size(); }

  /**
   * Retrieves the sum of the size of all table entries.
   */
  size_t size() const noexcept { return size_; }

  /**
   * Retrieves the maximum size the table may use.
   *
   * @see size() const noexcept
   */
  size_t maxSize() const noexcept { return maxSize_; }

  /**
   * Sets the maximum allowed total table size.
   *
   * @see size() const noexcept
   */
  void setMaxSize(size_t limit);

  /**
   * Adds given @p name and @p value pair to the table.
   */
  void add(const std::string& name, const std::string& value);

  /**
   * Adds given table @p entry to the table.
   */
  void add(const TableEntry& entry);

  /**
   * Searches for given @p field in the dynamic table.
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
  bool find(const TableEntry& entry,
            size_t* index,
            bool* nameValueMatch) const;

  /**
   * Searches for given @p field in the dynamic table.
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
  bool find(const std::string& name,
            const std::string& value,
            size_t* index,
            bool* nameValueMatch) const;

  const TableEntry& at(size_t index) const;

  void clear();

 protected:
  void evict();

 private:
  size_t maxSize_;
  size_t size_;

  std::deque<TableEntry> entries_;
};

} // namespace hpack
} // namespace http
} // namespace xzero

