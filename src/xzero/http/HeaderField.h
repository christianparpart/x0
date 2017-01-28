// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <string>

namespace xzero {
namespace http {

typedef std::string HeaderFieldName;
typedef std::string HeaderFieldValue;

/**
 * Represents a single HTTP message header name/value pair.
 */
class XZERO_HTTP_API HeaderField {
 public:
  static HeaderField parse(const std::string& field);

  HeaderField();
  HeaderField(HeaderField&&) = default;
  HeaderField(const HeaderField&) = default;
  HeaderField& operator=(HeaderField&&) = default;
  HeaderField& operator=(const HeaderField&) = default;
  HeaderField(const std::string& name, const std::string& value);
  HeaderField(const std::string& name, const std::string& value, bool sensitive);
  HeaderField(const std::pair<std::string, std::string>& field);

  const std::string& name() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const std::string& value() const { return value_; }
  void setValue(const std::string& value) { value_ = value; }

  /**
   * Flag that determines whether or not the underlying protocol
   * should treat this HeaderField with special security (at potential
   * cost of performance / network latency).
   */
  bool isSensitive() const noexcept { return sensitive_; }

  /**
   * Toggles whether or not the underlying transport protocol should
   * treat this HeaderField with special security (at potential
   * cost of performance / network latency).
   */
  void setSensitive(bool value) { sensitive_ = value; }

  void prependValue(const std::string& value, const std::string& delim = "") {
    if (value_.empty()) {
      value_ = value;
    } else {
      value_ = value + delim + value_;
    }
  }

  void appendValue(const std::string& value, const std::string& delim = "") {
    if (value_.empty()) {
      value_ = value;
    } else {
      value_ += delim;
      value_ += value;
    }
  }

  /** Performs an case-insensitive compare on name and value for equality. */
  bool operator==(const HeaderField& other) const;

  /** Performs an case-insensitive compare on name and value for inequality. */
  bool operator!=(const HeaderField& other) const;

 private:
  std::string name_;
  std::string value_;
  bool sensitive_;
};

// {{{ inlines
inline HeaderField::HeaderField()
    : name_(), value_(), sensitive_(false) {
}

inline HeaderField::HeaderField(const std::pair<std::string, std::string>& field)
    : HeaderField(field.first, field.second) {
}

inline HeaderField::HeaderField(const std::string& name,
                                const std::string& value)
    : HeaderField(name, value, false) {
}

inline HeaderField::HeaderField(const std::string& name,
                                const std::string& value,
                                bool sensitive)
    : name_(name), value_(value), sensitive_(sensitive) {
}
// }}}

XZERO_HTTP_API std::string inspect(const HeaderField& field);

}  // namespace http
}  // namespace xzero
