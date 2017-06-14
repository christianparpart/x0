// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <string>
#include <vector>
#include <utility>

namespace xzero {

/**
 * Represents a parsed URI.
 */
class XZERO_BASE_API Uri {
public:
  typedef std::vector<std::pair<std::string, std::string>> ParamList;

  static std::string encode(const std::string& str);
  static std::string decode(const std::string& str);

  Uri();
  explicit Uri(const std::string& uri);

  void parse(const std::string& uri);

  void setPath(const std::string& value);

  const std::string& scheme() const;
  const std::string& userinfo() const;
  const std::string& host() const;
  unsigned port() const;
  std::string hostAndPort() const;
  const std::string& path() const;
  const std::string& query() const;
  std::string pathAndQuery() const;
  ParamList queryParams() const;
  const std::string& fragment() const;
  std::string toString() const;

  static void parseURI(
      const std::string& uri_str,
      std::string* scheme,
      std::string* userinfo,
      std::string* host,
      unsigned* port,
      std::string* path,
      std::string* query,
      std::string* fragment);

  static void parseQueryString(const std::string& query, ParamList* params);

  static bool getParam(
      const ParamList&,
      const std::string& key,
      std::string* value);

protected:
  std::string scheme_;
  std::string userinfo_;
  std::string host_;
  unsigned port_;
  std::string path_;
  std::string query_;
  std::string fragment_;
};

} // namespace xzero
