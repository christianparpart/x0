// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_flow_RegExp_h
#define sw_flow_RegExp_h

#include <base/Api.h>
#include <pcre.h>
#include <string>
#include <vector>

namespace base {

class BufferRef;

class BASE_API RegExp {
 private:
  std::string pattern_;
  pcre* re_;

 public:
  typedef std::vector<std::pair<const char*, size_t>> Result;

 public:
  explicit RegExp(const std::string& pattern);
  RegExp();
  RegExp(const RegExp& v);
  ~RegExp();

  RegExp(RegExp&& v);
  RegExp& operator=(RegExp&& v);

  bool match(const char* buffer, size_t size, Result* result = nullptr) const;
  bool match(const BufferRef& buffer, Result* result = nullptr) const;
  bool match(const char* cstring, Result* result = nullptr) const;

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

class BASE_API RegExpContext {
 public:
  RegExpContext();
  virtual ~RegExpContext();

  RegExp::Result* regexMatch() {
    if (!regexMatch_) regexMatch_ = new RegExp::Result();

    return regexMatch_;
  }

 private:
  RegExp::Result* regexMatch_;
};

}  // namespace base

namespace std {
inline std::ostream& operator<<(std::ostream& os, const base::RegExp& re) {
  os << re.pattern();
  return os;
}
}

#endif
