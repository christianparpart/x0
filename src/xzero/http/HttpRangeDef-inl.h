// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <cstdlib>

namespace xzero::http {

inline HttpRangeDef::HttpRangeDef() :
    unitName_(),
    ranges_() {
}

inline HttpRangeDef::HttpRangeDef(const BufferRef& spec) :
    unitName_(),
    ranges_() {
  parse(spec);
}

inline void HttpRangeDef::push_back(std::size_t offset1, std::size_t offset2) {
  ranges_.push_back(std::make_pair(offset1, offset2));
}

inline void HttpRangeDef::push_back(
    const std::pair<std::size_t, std::size_t>& range) {
  ranges_.push_back(range);
}

inline std::size_t HttpRangeDef::size() const {
  return ranges_.size();
}

inline bool HttpRangeDef::empty() const { return !ranges_.size(); }

inline const HttpRangeDef::element_type& HttpRangeDef::operator[](
    std::size_t index) const {
  return ranges_[index];
}

inline HttpRangeDef::iterator HttpRangeDef::begin() { return ranges_.begin(); }

inline HttpRangeDef::iterator HttpRangeDef::end() { return ranges_.end(); }

inline HttpRangeDef::const_iterator HttpRangeDef::begin() const {
  return ranges_.begin();
}

inline HttpRangeDef::const_iterator HttpRangeDef::end() const {
  return ranges_.end();
}

inline std::string HttpRangeDef::str() const {
  std::stringstream sstr;
  int count = 0;

  sstr << unitName_;

  for (const_iterator i = begin(), e = end(); i != e; ++i) {
    if (count++) {
      sstr << ", ";
    }

    if (i->first != npos) {
      sstr << i->first;
    }

    sstr << '-';

    if (i->second != npos) {
      sstr << i->second;
    }
  }

  return sstr.str();
}

inline bool HttpRangeDef::parseByteRangeDef(const BufferRef& range) {
  return parseByteRangeDef(range.begin(), range.end());
}

}  // namespace xzero::http

// vim:syntax=cpp
