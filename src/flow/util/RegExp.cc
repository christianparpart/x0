// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/util/RegExp.h>
#include <cstring>
#include <regex>

namespace xzero::flow::util {

RegExp::RegExp()
    : pattern_(),
      re_() {
}

RegExp::RegExp(const RegExp& v)
    : pattern_(v.pattern_), re_(v.re_) {
}

RegExp::RegExp(const std::string& pattern)
    : pattern_(pattern), re_(pattern) {
}

RegExp::~RegExp() {
}

RegExp::RegExp(RegExp&& v)
    : pattern_(std::move(v.pattern_)),
      re_(std::move(v.re_)) {
}

RegExp& RegExp::operator=(RegExp&& v) {
  pattern_ = std::move(v.pattern_);
  re_ = std::move(v.re_);
  return *this;
}

bool RegExp::match(const std::string& target, Result* result) const {
  std::regex_search(target, *result, re_);
  return !result->empty();
}

const char* RegExp::c_str() const {
  return pattern_.c_str();
}

}  // namespace xzero::flow::util
