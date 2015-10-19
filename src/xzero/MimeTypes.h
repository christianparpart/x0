// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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

  /** Retrieves the default mimetype. */
  const std::string& defaultMimeType() const XZERO_BASE_NOEXCEPT;

  /** Sets the default mimetype to given @p value. */
  void setDefaultMimeType(const std::string& value);

  /** Retrieves a mimetype based on given file @p path name. */
  const std::string& getMimeType(const std::string& path);

  /** Retrieves the mimetype mappings (from file extension to mimetype). */
  const std::unordered_map<std::string, std::string>& mimetypes() const XZERO_BASE_NOEXCEPT;

 private:
  std::unordered_map<std::string, std::string> mimetypes_;
  std::string defaultMimeType_;
};

// {{{ inlines
inline const std::unordered_map<std::string, std::string>&
    MimeTypes::mimetypes() const XZERO_BASE_NOEXCEPT {
  return mimetypes_;
}

inline const std::string& MimeTypes::defaultMimeType() const XZERO_BASE_NOEXCEPT {
  return defaultMimeType_;
}

inline void MimeTypes::setDefaultMimeType(const std::string& value) {
  defaultMimeType_ = value;
}
// }}}

}  // namespace xzero
