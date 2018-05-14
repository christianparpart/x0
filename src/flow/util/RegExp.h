// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <regex>
#include <string>
#include <vector>
#include <memory>

namespace xzero::flow::util {

class BufferRef;

class RegExp {
 private:
  std::string pattern_;
  std::regex re_;

 public:
  using Result = std::smatch;

 public:
  explicit RegExp(const std::string& pattern);
  RegExp();
  RegExp(const RegExp& v);
  ~RegExp();

  RegExp(RegExp&& v);
  RegExp& operator=(RegExp&& v);

  bool match(const std::string& target, Result* result = nullptr) const;

  const std::string& pattern() const { return pattern_; }
  const char* c_str() const;

  operator const std::string&() const { return pattern_; }

  friend bool operator==(const RegExp& a, const RegExp& b) {
    return a.pattern() == b.pattern();
  }
  friend bool operator!=(const RegExp& a, const RegExp& b) {
    return a.pattern() != b.pattern();
  }
  friend bool operator<=(const RegExp& a, const RegExp& b) {
    return a.pattern() <= b.pattern();
  }
  friend bool operator>=(const RegExp& a, const RegExp& b) {
    return a.pattern() >= b.pattern();
  }
  friend bool operator<(const RegExp& a, const RegExp& b) {
    return a.pattern() < b.pattern();
  }
  friend bool operator>(const RegExp& a, const RegExp& b) {
    return a.pattern() > b.pattern();
  }
};

class RegExpContext {
 public:
  const RegExp::Result* regexMatch() const {
    if (!regexMatch_)
      regexMatch_ = std::make_unique<RegExp::Result>();

    return regexMatch_.get();
  }

  RegExp::Result* regexMatch() {
    if (!regexMatch_)
      regexMatch_ = std::make_unique<RegExp::Result>();

    return regexMatch_.get();
  }

 private:
  mutable std::unique_ptr<RegExp::Result> regexMatch_;
};

}  // namespace xzero::flow::util
