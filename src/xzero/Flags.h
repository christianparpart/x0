// This file is part of the "x0" project, // http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/net/IPAddress.h>

#include <optional>
#include <functional>
#include <system_error>
#include <unordered_map>
#include <vector>
#include <list>
#include <string>
#include <utility>

namespace xzero {

class Flags {
 public:
  enum class FlagType {
    String,
    Number,
    Float,
    IP,
    Bool,
  };

  // FlagPassingStyle
  enum FlagStyle {
    ShortSwitch,
    LongSwitch,
    ShortWithValue,
    LongWithValue,
    UnnamedParameter
  };

  enum class ErrorCode {
    TypeMismatch,
    UnknownOption,
    MissingOption,
    MissingOptionValue,
    NotFound,
  };
  
  class Error : public std::runtime_error {
   public:
    Error(ErrorCode code, std::string arg);

    ErrorCode code() const noexcept { return code_; }
    const std::string& arg() const noexcept { return arg_; }

   private:
    ErrorCode code_;
    std::string arg_;
  };

  struct FlagDef;
  class Flag;

  Flags();

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

  void set(const Flag& flag);
  void set(const std::string& opt, const std::string& val, FlagStyle fs, FlagType ft);
  bool isSet(const std::string& flag) const;

  Flags& defineString(const std::string& longOpt,
                      char shortOpt,
                      const std::string& valuePlaceholder,
                      const std::string& helpText,
                      std::optional<std::string> defaultValue = std::nullopt,
                      std::function<void(const std::string&)> callback = nullptr);

  Flags& defineNumber(const std::string& longOpt,
                      char shortOpt,
                      const std::string& valuePlaceholder,
                      const std::string& helpText,
                      std::optional<long int> defaultValue = std::nullopt,
                      std::function<void(long int)> callback = nullptr);

  Flags& defineFloat(const std::string& longOpt,
                     char shortOpt,
                     const std::string& valuePlaceholder,
                     const std::string& helpText,
                     std::optional<float> defaultValue = std::nullopt,
                     std::function<void(float)> callback = nullptr);

  Flags& defineIPAddress(const std::string& longOpt,
                         char shortOpt,
                         const std::string& valuePlaceholder,
                         const std::string& helpText,
                         std::optional<IPAddress> defaultValue = std::nullopt,
                         std::function<void(const IPAddress&)> callback = nullptr);

  Flags& defineBool(const std::string& longOpt,
                    char shortOpt,
                    const std::string& helpText,
                    std::function<void(bool)> callback = nullptr);

  Flags& enableParameters(const std::string& valuePlaceholder,
                          const std::string& helpText);

  std::string helpText() const { return helpText(78, 30); }
  std::string helpText(size_t width, size_t helpTextOffset) const;

  const FlagDef* findDef(const std::string& longOption) const;
  const FlagDef* findDef(char shortOption) const;

  void parse(int argc, const char* argv[]);
  void parse(const std::vector<std::string>& args);

  // Attempts to parse given arguments and returns an error code in case of parsing errors instead
  // of throwing.
  std::error_code tryParse(const std::vector<std::string>& args);

 private:
  Flags& define(const std::string& longOpt,
                char shortOpt,
                bool required,
                FlagType type,
                const std::string& helpText,
                const std::string& valuePlaceholder,
                const std::optional<std::string>& defaultValue,
                std::function<void(const std::string&)> callback);

 private:
  std::list<FlagDef> flagDefs_;
  bool parametersEnabled_;              // non-option parameters enabled?
  std::string parametersPlaceholder_;
  std::string parametersHelpText_;

  typedef std::pair<FlagType, std::string> FlagValue;
  std::unordered_map<std::string, FlagValue> set_;
  std::vector<std::string> raw_;
};

struct Flags::FlagDef {
  FlagType type;
  std::string longOption;
  char shortOption;
  bool required;
  std::string valuePlaceholder;
  std::string helpText;
  std::optional<std::string> defaultValue;
  std::function<void(const std::string&)> callback;

  std::string makeHelpText(size_t width, size_t helpTextOffset) const;
};

class Flags::Flag {
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

class FlagsErrorCategory : public std::error_category {
 public:
  static FlagsErrorCategory& get();

  const char* name() const noexcept override;
  std::string message(int ec) const override;
};

std::error_code make_error_code(Flags::ErrorCode errc);

// maybe via CLI / FlagBuilder
std::string inspect(const Flags& flags);

}  // namespace xzero

namespace std {
  template<> struct is_error_code_enum<xzero::Flags::ErrorCode> : public std::true_type {};
} // namespace std
