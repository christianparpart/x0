// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HeaderFieldList.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <algorithm>

namespace xzero {
namespace http {

HeaderFieldList::HeaderFieldList(
    const std::initializer_list<std::pair<std::string, std::string>>& init) {

  for (const auto& field : init) {
    push_back(field.first, field.second);
  }
}

HeaderFieldList::HeaderFieldList(const std::vector<std::pair<std::string, std::string>>& init) {
  for (const auto& field: init) {
    push_back(field.first, field.second);
  }
}

void HeaderFieldList::push_back(const HeaderFieldList& list) {
  for (const HeaderField& field: list)
    push_back(field.name(), field.value());
}

void HeaderFieldList::push_back(HeaderFieldList&& list) {
  for (HeaderField& field: list)
    push_back(std::move(field));
}

void HeaderFieldList::push_back(const HeaderField& field) {
  if (field.name().empty())
    RAISE(RuntimeError, "Invalid field name.");

  entries_.emplace_back(field);
}

void HeaderFieldList::push_back(HeaderField&& field) {
  if (field.name().empty())
    RAISE(RuntimeError, "Invalid field name.");

  entries_.emplace_back(std::move(field));
}

void HeaderFieldList::push_back(const std::string& name,
                                const std::string& value) {
  if (name.empty())
    RAISE(RuntimeError, "Invalid field name.");

  entries_.emplace_back(name, value);
}

void HeaderFieldList::push_back(const std::string& name,
                                const std::string& value,
                                bool sensitive) {
  if (name.empty())
    RAISE(RuntimeError, "Invalid field name.");

  entries_.emplace_back(name, value, sensitive);
}

void HeaderFieldList::overwrite(const std::string& name,
                                const std::string& value) {
  if (name.empty())
    RAISE(RuntimeError, "Invalid field name.");

  remove(name);
  push_back(name, value);
}

void HeaderFieldList::prepend(const std::string& name,
                              const std::string& value,
                              const std::string& delim) {
  for (HeaderField& field : entries_) {
    if (iequals(field.name(), name)) {
      field.prependValue(value, delim);
      return;
    }
  }

  push_back(name, value);
}

void HeaderFieldList::append(const std::string& name,
                             const std::string& value,
                             const std::string& delim) {
  for (HeaderField& field : entries_) {
    if (iequals(field.name(), name)) {
      field.appendValue(value, delim);
      return;
    }
  }

  push_back(name, value);
}

void HeaderFieldList::remove(const std::string& name) {
  auto start = begin();
  while (start != end()) {
    auto i = std::find_if(begin(), end(), [&](const HeaderField& field) {
      return iequals(field.name(), name);
    });

    if (i != end()) {
      entries_.erase(i);
    }
    start = i;
  }
}

bool HeaderFieldList::contains(const std::string& name) const {
  for (const HeaderField& field : entries_) {
    if (iequals(field.name(), name)) {
      return true;
    }
  }

  return false;
}

bool HeaderFieldList::contains(const std::string& name,
                               const std::string& value) const {
  for (const HeaderField& field : entries_) {
    if (iequals(field.name(), name) && iequals(field.value(), value)) {
      return true;
    }
  }

  return false;
}

const std::string& HeaderFieldList::get(const std::string& name) const {
  for (const HeaderField& field : entries_) {
    if (iequals(field.name(), name)) {
      return field.value();
    }
  }
  static std::string notfound;
  return notfound;
}

const std::string& HeaderFieldList::operator[](const std::string& name) const {
  return get(name);
}

const HeaderField& HeaderFieldList::operator[](size_t index) const {
  if (index >= entries_.size())
    RAISE(IndexError, "Index out of bounds");

  auto i = entries_.begin();
  auto e = entries_.end();

  while (i != e && index > 0) {
    i++;
    index--;
  }

  return *i;
}

void HeaderFieldList::reset() {
  entries_.clear();
}

}  // namespace http
}  // namespace xzero
