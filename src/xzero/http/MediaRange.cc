// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/http/MediaRange.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <list>
#include <stdlib.h>

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.MediaRange: " msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero::http {

MediaRange::MediaRange(const std::string& type,
             const std::string& subtype,
             const std::unordered_map<std::string, std::string>& parameters)
    : type_(type),
      subtype_(subtype),
      qualityCache_(-1),
      parameters_(parameters) {
}

double MediaRange::quality() const {
  if (qualityCache_ < 0) {
    auto i = parameters_.find("q");
    if (i == parameters_.end())
      return 1.0;

    qualityCache_ = strtod(i->second.c_str(), nullptr);
  }

  return qualityCache_;
}

const std::string* MediaRange::getParameter(const std::string& name) const {
  auto i = parameters_.find(name);

  if (i == parameters_.end())
    return nullptr;

  return &i->second;
}

bool MediaRange::contains(const std::string& mediatype) const {
  size_t p = mediatype.find('/');
  if (p == std::string::npos)
    return false;

  auto type = mediatype.substr(0, p);
  if (type != type_ && type_ != "*")
    return false;

  auto subtype = mediatype.substr(p + 1);
  if (subtype != subtype_ && subtype_ != "*")
    return false;

  return true;
}

bool MediaRange::operator==(const std::string& mediatype) const {
  return contains(mediatype);
}

bool MediaRange::operator!=(const std::string& mediatype) const {
  return !contains(mediatype);
}

Result<MediaRange> MediaRange::parse(const std::string& range) {
  // https://tools.ietf.org/html/rfc7231#section-5.3.2
  //
  // media-range    = ( "*/*"
  //                  / ( type "/" "*" )
  //                  / ( type "/" subtype )
  //                  ) *( OWS ";" OWS parameter )
  // accept-params  = weight *( accept-ext )
  // accept-ext = OWS ";" OWS token [ "=" ( token / quoted-string ) ]

  // TODO: use a proper parser instead of this substring-kungfu

  size_t i = range.find('/');
  if (i == std::string::npos)
    return std::errc::invalid_argument;

  std::string type = range.substr(0, i);
  i++;

  std::string subtype;
  std::unordered_map<std::string, std::string> parameters;
  size_t k = range.find(';', i);
  if (k == std::string::npos) {
    subtype = range.substr(i);
  } else {
    subtype = range.substr(i, k - i);
    k++;
    while (!subtype.empty() && std::isspace(subtype[subtype.size() - 1]))
      subtype.resize(subtype.size() - 1);

    // parse parameters
    std::string paramstr = range.substr(k);
    std::vector<std::string> params = StringUtil::split(range.substr(k), ";");

    for (const auto& param: params) {
      auto trimmed = StringUtil::trim(param);
      std::vector<std::string> pair = StringUtil::split(trimmed, "=");
      if (pair.size() != 2)
        return std::errc::invalid_argument;

      pair[0] = StringUtil::trim(pair[0]);
      pair[1] = StringUtil::trim(pair[1]);

      parameters[StringUtil::toLower(pair[0])] = pair[1];
    }
  }

  return MediaRange(StringUtil::toLower(type),
                    StringUtil::toLower(subtype),
                    parameters);
}

Result<std::vector<MediaRange>> MediaRange::parseMany(const std::string& ranges) {
  std::vector<std::string> rangeList = StringUtil::split(ranges, ",");

  std::vector<MediaRange> result;

  for (const std::string& rangeSpec: rangeList) {
    Result<MediaRange> parsed = parse(StringUtil::trim(rangeSpec));
    if (parsed.error())
      return parsed.error();
    else
      result.emplace_back(std::move(*parsed));
  }

  return result;
}

const MediaRange* MediaRange::match(const std::vector<MediaRange>& accepts,
                                    const std::vector<std::string>& available) {
  std::list<const MediaRange*> matched;

  for (const std::string& avail: available)
    for (const auto& accept: accepts)
      if (accept.contains(avail))
        matched.push_back(&accept);

  if (matched.empty())
    return nullptr;

  // find best quality
  const MediaRange* bestMatch = matched.front();
  matched.pop_front();
  for (const MediaRange* match: matched)
    if (match->quality() > bestMatch->quality())
      bestMatch = match;

  return bestMatch;
}

std::string MediaRange::match(const std::string& acceptsStr,
                              const std::vector<std::string>& available) {
  std::vector<MediaRange> accepts = *MediaRange::parseMany(acceptsStr);
  if (auto best = MediaRange::match(accepts, available))
    return std::string(best->type() + "/" + best->subtype());
  else
    return std::string();
}

} // namespace xzero::http
