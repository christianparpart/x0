// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/MimeTypes.h>
#include <xzero/io/FileUtil.h>
#include <xzero/StringUtil.h>
#include <xzero/Buffer.h>

namespace xzero {

MimeTypes::MimeTypes()
    : MimeTypes("", "application/octet-stream") {
}

MimeTypes::MimeTypes(const std::string& path,
                     const std::string& defaultMimeType)
    : mimetypes_(),
      defaultMimeType_(defaultMimeType) {

  if (!path.empty()) {
    loadFromLocal(path);
  }
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
  std::string* result = nullptr;

  // query mimetype
  const size_t ndot = path.find_last_of(".");
  const size_t nslash = path.find_last_of("/");

  if (ndot != std::string::npos && ndot > nslash) {
    std::string ext(path.substr(ndot + 1));

    while (ext.size()) {
      auto i = mimetypes_.find(ext);

      if (i != mimetypes_.end())
        result = &i->second;

      if (ext[ext.size() - 1] != '~')
        break;

      ext.resize(ext.size() - 1);
    }

    if (!result || result->empty()) {
      result = &defaultMimeType_;
    }
  } else {
    result = &defaultMimeType_;
  }

  return *result;
}

bool MimeTypes::empty() const noexcept {
  return mimetypes_.empty();
}

void MimeTypes::load(const std::unordered_map<std::string, std::string>& entries) {
  mimetypes_ = entries;
}

}  // namespace xzero
