// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <map>

namespace xzero {

class IniFile {
 public:
  typedef std::map<std::string, std::string> Section;
  typedef std::map<std::string, Section> SectionMap;

  typedef SectionMap::iterator iterator;
  typedef SectionMap::const_iterator const_iterator;

 public:
  IniFile();
  ~IniFile();

  /// loads config settings from given filename.
  void load(const std::string& contents);

  /// serializes config object into INI style config file format
  std::string serialize() const;

  /// completely clears all config data.
  void clear();

  /// tests wether given section exists.
  bool contains(const std::string& title) const;

  /// gets all values of given section.
  Section get(const std::string& title) const;

  /// removes a section from this config object
  void remove(const std::string& title);

  /// tests wether given data key in given section exists.
  bool contains(const std::string& title, const std::string& key) const;

  /// gets value of given section->key pair.
  std::string get(const std::string& title, const std::string& key) const;

  bool get(const std::string& title, const std::string& key,
           std::string& result) const;

  /// sets value of given section->key pair.
  std::string set(const std::string& title, const std::string& key,
                  const std::string& value);

  /// loads given \p key from given \p title into \p result.
  bool load(const std::string& title, const std::string& key,
            std::string& result) const;

  /// removes given data by key from given section.
  void remove(const std::string& title, const std::string& key);

  iterator begin() const { return sections_.begin(); }
  iterator end() const { return sections_.end(); }

  const_iterator cbegin() const { return sections_.cbegin(); }
  const_iterator cend() const { return sections_.cend(); }

  size_t size() const { return sections_.size(); }

 private:
  mutable SectionMap sections_;
};

}  // namespace xzero
