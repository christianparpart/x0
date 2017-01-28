// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/cli/Flags.h>
#include <xzero/cli/CLI.h>
#include <xzero/net/IPAddress.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <sstream>

extern char** environ;

namespace xzero {

// {{{ Flag
Flag::Flag(
    const std::string& opt,
    const std::string& val,
    FlagStyle fs,
    FlagType ft)
    : type_(ft),
      style_(fs),
      name_(opt),
      value_(val) {
}
// }}}

Flags::Flags() {
}

void Flags::merge(const std::vector<Flag>& args) {
  for (const Flag& arg: args)
    merge(arg);
}

void Flags::merge(const Flag& flag) {
  set_[flag.name()] = std::make_pair(flag.type(), flag.value());
}

bool Flags::isSet(const std::string& flag) const {
  return set_.find(flag) != set_.end();
}

IPAddress Flags::getIPAddress(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    RAISE_STATUS(CliFlagNotFoundError);

  if (i->second.first != FlagType::IP)
    RAISE_STATUS(CliTypeMismatchError);

  return IPAddress(i->second.second);
}

std::string Flags::asString(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    RAISE_STATUS(CliFlagNotFoundError);

  return i->second.second;
}

std::string Flags::getString(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    RAISE_STATUS(CliFlagNotFoundError);

  if (i->second.first != FlagType::String)
    RAISE_STATUS(CliTypeMismatchError);

  return i->second.second;
}

long int Flags::getNumber(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    RAISE_STATUS(CliFlagNotFoundError);

  if (i->second.first != FlagType::Number)
    RAISE_STATUS(CliTypeMismatchError);

  return std::stoi(i->second.second);
}

float Flags::getFloat(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    RAISE_STATUS(CliFlagNotFoundError);

  if (i->second.first != FlagType::Float)
    RAISE_STATUS(CliTypeMismatchError);

  return std::stof(i->second.second);
}

bool Flags::getBool(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    return false;

  return i->second.second == "true";
}

const std::vector<std::string>& Flags::parameters() const {
  return raw_;
}

void Flags::setParameters(const std::vector<std::string>& v) {
  raw_ = v;
}

std::string Flags::to_s() const {
  std::stringstream sstr;

  int i = 0;
  for (const auto& flag: set_) {
    if (i)
      sstr << ' ';

    i++;

    switch (flag.second.first) {
      case FlagType::Bool:
        if (flag.second.second == "true")
          sstr << "--" << flag.first;
        else
          sstr << "--" << flag.first << "=false";
        break;
      case FlagType::String:
        sstr << "--" << flag.first << "=\"" << flag.second.second << "\"";
        break;
      default:
        sstr << "--" << flag.first << "=" << flag.second.second;
        break;
    }
  }

  return sstr.str();
}

std::string inspect(const Flags& flags) {
  return flags.to_s();
}

}  // namespace xzero
