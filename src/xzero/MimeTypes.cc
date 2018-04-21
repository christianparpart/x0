// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Buffer.h>
#include <xzero/MimeTypes.h>
#include <xzero/StringUtil.h>
#include <xzero/io/FileUtil.h>
#include <xzero/logging.h>

namespace xzero {

MimeTypes::MimeTypes()
    : defaultMimeType_{},
      mimetypes_{} {
}

MimeTypes::MimeTypes(const std::string& defaultMimeType,
                     const std::unordered_map<std::string, std::string>& entries)
    : defaultMimeType_{defaultMimeType},
      mimetypes_{entries} {
}

MimeTypes::MimeTypes(const std::string& defaultMimeType,
                     const std::string& path)
    : defaultMimeType_{defaultMimeType},
      mimetypes_{} {
  loadFromLocal(path);
}

void MimeTypes::loadFromLocal(const std::string& path) {
  std::string input = FileUtil::read(path).str();
  loadFromString(input);
}

void MimeTypes::loadFromString(const std::string& input) {
  mimetypes_.clear();
  auto lines = StringUtil::split(input, "\n");

  for (auto line : lines) {
    line = StringUtil::trim(line);
    auto columns = StringUtil::splitByAny(line, " \t");

    auto ci = columns.begin(), ce = columns.end();
    std::string mime = ci != ce ? *ci++ : std::string();

    if (!mime.empty() && mime[0] != '#') {
      for (; ci != ce; ++ci) {
        mimetypes_[*ci] = mime;
      }
    }
  }
}

void MimeTypes::setMimeType(const std::string& ext,
                            const std::string& mimetype) {
  mimetypes_[ext] = mimetype;
}

const std::string& MimeTypes::getMimeType(const std::string& path) {
  if (path.empty())
    return defaultMimeType_;

  // treat '~'-backup files special, as they work w/o a dot
  if (path[path.size() - 1] == '~') {
    static const std::string trash = "application/x-trash";
    return trash;
  }

  // query mimetype
  const size_t ndot = path.find_last_of(".");
  const size_t nslash = path.find_last_of("/");

  if (ndot == std::string::npos || nslash == std::string::npos || ndot < nslash)
    return defaultMimeType_;

  std::string ext(path.substr(ndot + 1));

  if (const auto& i = mimetypes_.find(ext); i != mimetypes_.end())
    return i->second;

  return defaultMimeType_;
}

void MimeTypes::load(const std::unordered_map<std::string, std::string>& entries) {
  mimetypes_ = entries;
}

}  // namespace xzero
