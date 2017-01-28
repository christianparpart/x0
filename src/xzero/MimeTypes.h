// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <unordered_map>
#include <string>

namespace xzero {

class XZERO_BASE_API MimeTypes {
 public:
  MimeTypes();
  MimeTypes(const std::string& path, const std::string& defaultMimeType);

  /** Loads the mimetype map from given local file at @p path. */
  void loadFromLocal(const std::string& path);

  /** Loads the mimetype map from given string with format from a mime.types file. */
  void loadFromString(const std::string& value);

  /** Retrieves the default mimetype. */
  const std::string& defaultMimeType() const XZERO_NOEXCEPT;

  /** Sets the default mimetype to given @p value. */
  void setDefaultMimeType(const std::string& value);

  /** Retrieves a mimetype based on given file @p path name. */
  const std::string& getMimeType(const std::string& path);

  /**
   * Assigns given @p mimetype to the file extension @p ext.
   */
  void setMimeType(const std::string& ext, const std::string& mimetype);

  /**
   * Tests whether mimetypes database is empty or not.
   */
  bool empty() const noexcept;

  /**
   * Initializes mimetypes DB with given entries.
   */
  void load(const std::unordered_map<std::string, std::string>& entries);

  /** Retrieves the mimetype mappings (from file extension to mimetype). */
  const std::unordered_map<std::string, std::string>& mimetypes() const XZERO_NOEXCEPT;

 private:
  std::unordered_map<std::string, std::string> mimetypes_;
  std::string defaultMimeType_;
};

// {{{ inlines
inline const std::unordered_map<std::string, std::string>&
    MimeTypes::mimetypes() const XZERO_NOEXCEPT {
  return mimetypes_;
}

inline const std::string& MimeTypes::defaultMimeType() const XZERO_NOEXCEPT {
  return defaultMimeType_;
}

inline void MimeTypes::setDefaultMimeType(const std::string& value) {
  defaultMimeType_ = value;
}
// }}}

}  // namespace xzero
