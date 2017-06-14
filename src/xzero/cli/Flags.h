// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/cli/FlagType.h>
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include <utility>

namespace xzero {

class IPAddress;

// FlagPassingStyle
enum FlagStyle {
  ShortSwitch,
  LongSwitch,
  ShortWithValue,
  LongWithValue,
  UnnamedParameter
};

class XZERO_BASE_API Flag {
 public:
  Flag(
      const std::string& opt,
      const std::string& val,
      FlagStyle fs,
      FlagType ft);

  explicit Flag(char shortOpt);
  Flag(char shortOpt, const std::string& val);
  Flag(const std::string& longOpt);
  Flag(const std::string& longOpt, const std::string& val);

  FlagType type() const { return type_; }
  const std::string& name() const { return name_; }
  const std::string& value() const { return value_; }

 private:
  FlagType type_;
  FlagStyle style_;
  std::string name_;
  std::string value_;
};

typedef std::pair<FlagType, std::string> FlagValue;

class XZERO_BASE_API Flags {
 public:
  Flags();

  void merge(const std::vector<Flag>& args);
  void merge(const Flag& flag);

  template<typename... Args>
  void set(Args... args) { merge(Flag(args...)); }
  bool isSet(const std::string& flag) const;

  IPAddress getIPAddress(const std::string& flag) const;
  std::string getString(const std::string& flag) const;
  std::string asString(const std::string& flag) const;
  long int getNumber(const std::string& flag) const;
  float getFloat(const std::string& flag) const;
  bool getBool(const std::string& flag) const;

  const std::vector<std::string>& parameters() const;
  void setParameters(const std::vector<std::string>& v);

  size_t size() const { return set_.size(); }

  std::string to_s() const;

 private:
  std::unordered_map<std::string, FlagValue> set_;
  std::vector<std::string> raw_;
};

// maybe via CLI / FlagBuilder
XZERO_BASE_API std::string inspect(const Flags& flags);

}  // namespace xzero
