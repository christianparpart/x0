// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/http/hpack/StaticTable.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace hpack {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.hpack.DynamicTable", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

DynamicTable::DynamicTable(size_t maxSize)
    : maxSize_(maxSize),
      size_(0),
      entries_() {
}

void DynamicTable::setMaxSize(size_t limit) {
  maxSize_ = limit;
  evict();
}

void DynamicTable::add(const std::string& name, const std::string& value) {
  entries_.emplace_front(name, value);
  size_ += name.size() + value.size() + HeaderFieldOverheadSize;
  evict();
}

void DynamicTable::add(const TableEntry& entry) {
  add(entry.first, entry.second);
}

bool DynamicTable::find(const TableEntry& entry,
                        size_t* index,
                        bool* nameValueMatch) const {
  return find(entry.first, entry.second, index, nameValueMatch);
}

bool DynamicTable::find(const std::string& name,
                        const std::string& value,
                        size_t* index,
                        bool* nameValueMatch) const {
  for (size_t i = 0, max = entries_.size(); i < max; i++) {
    if (name != entries_[i].first)
      continue;

    *index = i;
    *nameValueMatch = value == entries_[i].second;
    return true;
  }

  return false;
}

const TableEntry& DynamicTable::at(size_t index) const {
  return entries_[index];
}


void DynamicTable::evict() {
  size_t n = 0;

  while (size_ > maxSize_) {
    TRACE("evict: evicting last field as current size $0 > max size $1",
          size_, maxSize_);
    size_ -= (entries_.back().first.size() +
              entries_.back().second.size() +
              HeaderFieldOverheadSize);
    n++;

    entries_.pop_back();
  }

  if (n) {
    TRACE("evict: evicted $0 fields", n);
  }
}

void DynamicTable::clear() {
  entries_.clear();
  size_ = 0;
}

} // namespace hpack
} // namespace http
} // namespace xzero

