// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Uri.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>

namespace xzero {

std::string Uri::encode(const std::string& str) {
  logFatal("NotImplementedError");
}

std::string Uri::decode(const std::string& str) {
  std::string decoded;
  const char* begin = str.c_str();
  const char* end = begin + str.size();

  while (begin != end) {
    if (*begin == '%') {
      char hex[3];
      if (++begin + 2 > end) {
        throw std::invalid_argument{"Decoding error"};
      }
      hex[0] = *begin++;
      hex[1] = *begin++;
      hex[2] = 0;
      decoded += static_cast<char>(std::stoul(hex, nullptr, 16));
    } else {
      decoded += *begin++;
    }
  }

  return decoded;
}

Uri::Uri() : port_(0) {
}

Uri::Uri(const std::string& uri_str) : port_(0) {
  parse(uri_str);
}

void Uri::parse(const std::string& uri_str) {
  Uri::parseURI(
      uri_str,
      &scheme_,
      &userinfo_,
      &host_,
      &port_,
      &path_,
      &query_,
      &fragment_);
}

void Uri::setPath(const std::string& value) {
  path_ = value;
}

const std::string& Uri::scheme() const {
  return scheme_;
}

const std::string& Uri::userinfo() const {
  return userinfo_;
}

const std::string& Uri::host() const {
  return host_;
}

unsigned Uri::port() const {
  return port_;
}

std::string Uri::hostAndPort() const {
  if (port_ > 0) {
    return StringUtil::format("$0:$1", host_, port_);
  } else {
    return host_;
  }
}

const std::string& Uri::path() const {
  return path_;
}

std::string Uri::pathAndQuery() const {
  if (query_.length() > 0) {
    return StringUtil::format("$0?$1", path_, query_);
  } else {
    return path_;
  }
}

const std::string& Uri::query() const {
  return query_;
}

Uri::ParamList Uri::queryParams()
    const {
  Uri::ParamList params;

  if (query_.size() > 0) {
    Uri::parseQueryString(query_, &params);
  }

  return params;
}

bool Uri::getParam(
    const Uri::ParamList& params,
    const std::string& key,
    std::string* value) {
  for (const auto& param : params) {
    if (param.first == key) {
      *value = param.second;
      return true;
    }
  }

  return false;
}

const std::string& Uri::fragment() const {
  return fragment_;
}

std::string Uri::toString() const {
  std::string uri_str;

  uri_str.append(scheme());
  uri_str.append(":");

  if (host_.size() > 0) {
    uri_str.append("//");

    if (userinfo_.size() > 0) {
      uri_str.append(userinfo_);
      uri_str.append("@");
    }

    uri_str.append(host_);

    if (port_ > 0) { // FIXPAUL hasPort
      uri_str.append(":");
      uri_str.append(std::to_string(port_));
    }
  }

  uri_str.append(path_);

  if (query_.size() > 0) {
    uri_str.append("?");
    uri_str.append(query_);
  }

  if (fragment_.size() > 0) {
    uri_str.append("#");
    uri_str.append(fragment_);
  }

  return uri_str;
}

void Uri::parseURI(
    const std::string& uri_str,
    std::string* scheme,
    std::string* userinfo,
    std::string* host,
    unsigned* port,
    std::string* path,
    std::string* query,
    std::string* fragment) {
  const char* begin = uri_str.c_str();
  const char* end = begin + uri_str.size();

  /* scheme */
  bool has_scheme = false;
  for (const char* cur = begin; cur < end; ++cur) {
    if (*cur == '/') {
      break;
    }

    if (*cur == ':') {
      *scheme = std::string(begin, cur - begin);
      begin = cur + 1;
      has_scheme = true;
      break;
    }
  }

  /* authority */
  if (begin < end - 2 && begin[0] == '/' && begin[1] == '/') {
    begin += 2;
    const char* cur = begin;
    for (; cur < end && *cur != '/' && *cur != '?' && *cur != '#'; ++cur);
    if (cur > begin) {
      const char* abegin = begin;
      const char* aend = cur;

      /* userinfo */
      for (const char* acur = abegin; acur < aend; ++acur) {
        if (*acur == '/' || *acur == '?' || *acur == '#') {
          break;
        }

        if (*acur == '@') {
          *userinfo = std::string(abegin, acur - abegin);
          abegin = acur + 1;
          break;
        }
      }

      /* host */
      const char* acur = abegin;
      for (; acur < aend &&
            *acur != '/' &&
            *acur != '?' &&
            *acur != '#' &&
            *acur != ':'; ++acur);
      *host = std::string(abegin, acur - abegin);

      /* port */
      if (acur < aend - 1 && *acur == ':') {
        abegin = ++acur;
        for (; *acur >= '0' && *acur <= '9'; ++acur);
        if (acur > abegin) {
          std::string port_str(abegin, acur - abegin);
          try {
            *port = std::stoi(port_str);
          } catch (const std::exception& e) {
            throw std::invalid_argument{"Decoding error. Invalid URI port."};
          }
        }
      }
    }
    begin = cur;
  }

  /* path */
  if (begin < end) {
    const char* cur = begin;
    for (; cur < end && *cur != '?' && *cur != '#'; ++cur);
    if (cur > begin) {
      *path = std::string(begin, cur - begin);
    }
    begin = cur;
  }

  /* query */
  if (begin < end && *begin == '?') {
    const char* cur = ++begin;
    for (; cur < end && *cur != '#'; ++cur);
    if (cur > begin) {
      *query = std::string(begin, cur - begin);
    }
    begin = cur;
  }

  /* fragment */
  if (begin < end - 1 && *begin == '#') {
    *fragment = std::string(begin + 1, end - begin - 1);
  }
}

void Uri::parseQueryString(const BufferRef& query, ParamList* params) {
  parseQueryString(query.data(), query.size(), params);
}

void Uri::parseQueryString(const std::string& query, ParamList* params) {
  parseQueryString(query.data(), query.size(), params);
}

void Uri::parseQueryString(const char* query,
                           size_t queryLength,
                           ParamList* params) {
  const char* begin = query;
  const char* end = begin + queryLength;

  for (const char* cur = begin; cur < end; ++cur) {
    for (; cur < end && *cur != '=' && *cur != '&'; cur++);
    if (cur > begin && cur < end && *cur == '=' ) {
      std::string key_str(begin, cur - begin);
      const char* val = ++cur;
      for (; cur < end && *cur != '=' && *cur != '&'; cur++);
      if (cur > val) {
        std::string val_str(val, cur - val);
        params->emplace_back(Uri::decode(key_str), Uri::decode(val_str));
        begin = cur + 1;
      } else {
        break;
      }
    } else {
      break;
    }
  }
}

} // namespace xzero
