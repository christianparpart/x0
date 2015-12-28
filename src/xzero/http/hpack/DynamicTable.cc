// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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

void DynamicTable::add(const HeaderField& field) {
  entries_.emplace_front(field);
  size_ += field.name().size() + field.value().size() + HeaderFieldOverheadSize;
  evict();
}

size_t DynamicTable::find(const HeaderField& field,
                          bool* nameValueMatch) const {
  for (size_t index = 0, max = entries_.size(); index < max; index++) {
    if (field.name() != entries_[index].name())
      continue;

    *nameValueMatch = field.value() == entries_[index].value();
    return index;
  }

  return npos;
}

const HeaderField& DynamicTable::operator[](size_t index) const {
  return index < StaticTable::length()
      ? StaticTable::at(index)
      : entries_[index - StaticTable::length()];
}

void DynamicTable::evict() {
  size_t n = 0;

  while (size_ > maxSize_) {
    TRACE("evict: evicting last field as current size $0 > max size $1",
          size_, maxSize_);
    size_ -= (entries_.back().name().size() +
              entries_.back().value().size() +
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

