// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <regex>
#include <string>
#include <vector>

namespace xzero {

class BufferRef;

class RegExp {
 private:
  std::string pattern_;
  std::regex re_;

 public:
  typedef std::smatch Result;

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
  RegExp::Result* regexMatch() {
    if (!regexMatch_) {
      regexMatch_.reset(new RegExp::Result());
    }

    return regexMatch_.get();
  }

 private:
  std::unique_ptr<RegExp::Result> regexMatch_;
};

}  // namespace xzero

namespace std {
inline std::ostream& operator<<(std::ostream& os, const xzero::RegExp& re) {
  os << re.pattern();
  return os;
}
}
