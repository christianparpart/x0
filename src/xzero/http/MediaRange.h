// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <xzero/Result.h>

namespace xzero::http {

class MediaRange {
 public:
  MediaRange(const std::string& type,
             const std::string& subtype,
             const std::unordered_map<std::string, std::string>& parameters);
  MediaRange(const MediaRange&) = default;
  MediaRange(MediaRange&&) = default;
  MediaRange& operator=(const MediaRange&) = default;
  MediaRange& operator=(MediaRange&&) = default;

  const std::string& type() const noexcept { return type_; }
  const std::string& subtype() const noexcept { return subtype_; }
  double quality() const;

  const std::string* getParameter(const std::string& name) const;
  const std::unordered_map<std::string, std::string>& parameters() const { return parameters_; }

  bool contains(const std::string& mediatype) const;

  bool operator==(const std::string& mediatype) const;
  bool operator!=(const std::string& mediatype) const;

  static Result<MediaRange> parse(const std::string& range);

  static Result<std::vector<MediaRange>> parseMany(const std::string& ranges);

  /**
   * Matches available media ranges against @p accept'ed media ranges.
   *
   * @param accept e.g. the list of media ranges an HTTP client accepts.
   * @param available e.g. the list of media types an HTTP server supports.
   */
  static const MediaRange* match(const std::vector<MediaRange>& accept,
                                 const std::vector<std::string>& available);

  /**
   * Matches available media ranges against @p accept'ed media types.
   *
   * @param accept Comma seperated list of acceptable media ranges.
   * @param available List of supported media types.
   * @param defaultType media-type to return if @p accept is empty.
   *
   * @return first available media type if @p accept is empty,
   *         an error code on @p accept string parsing failure,
   *         the media-type that matched the best to @p accept,
   *         or empty string otherwise
   */
  static Result<std::string> match(const std::string& accept,
                                   const std::vector<std::string>& available,
                                   const std::string& defaultType);

 private:
  std::string type_;
  std::string subtype_;
  mutable double qualityCache_;
  std::unordered_map<std::string, std::string> parameters_;
};

} // namespace xzero::http
