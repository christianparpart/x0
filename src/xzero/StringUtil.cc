// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <string>
#include <sstream>
#include <iomanip>
#include <system_error>
#include <xzero/RuntimeError.h>
#include <xzero/BufferUtil.h>
#include <xzero/StringUtil.h>
#include <xzero/UTF8.h>

#if defined(XZERO_OS_WIN32)
#define strncasecmp _strnicmp
#endif

namespace xzero {

std::string StringUtil::trim(const std::string& value) {
#if defined(XZERO_OS_WIN32)
  auto isSpace = [](char p) { return p == ' ' || p == '\t'; };

  std::size_t left = 0;
  while (isSpace(value[left]))
    ++left;

  std::size_t right = value.size() - 1;
  while (isSpace(value[right]))
    --right;

  return value.substr(left, 1 + right - left);
#else
  std::size_t left = 0;
  while (std::isspace(value[left])) ++left;

  std::size_t right = value.size() - 1;
  while (std::isspace(value[right])) --right;

  return value.substr(left, 1 + right - left);
#endif
}

void StringUtil::stripTrailingSlashes(std::string* str) {
  while (str->back() == '/') {
    str->pop_back();
  }
}

void StringUtil::replaceAll(
    std::string* str,
    const std::string& pattern,
    const std::string& replacement) {
  if (str->size() == 0) {
    return;
  }

  size_t cur = 0;
  while((cur = str->find(pattern, cur)) != std::string::npos) {
    str->replace(cur, pattern.size(), replacement);
    cur += replacement.size();
  }
}

std::vector<std::string> StringUtil::splitByAny(
      const std::string& str,
      const std::string& pattern) {
  std::vector<std::string> parts;

  auto i = str.begin();
  auto e = str.end();

  do {
    // consume delimiters
    while (i != e && StringUtil::find(pattern, *i) != std::string::npos) {
      i++;
    }

    // consume token
    if (i != e) {
      auto begin = i;
      while (i != e && StringUtil::find(pattern, *i) == std::string::npos) {
        i++;
      }
      parts.emplace_back(begin, i);
    }
  } while (i != e);

  return parts;
}

std::vector<std::string> StringUtil::split(
      const std::string& str,
      const std::string& pattern) {
  std::vector<std::string> parts;

  if (str.empty())
    return {};

  size_t begin = 0;
  for (;;) {
    auto end = str.find(pattern, begin);

    if (end == std::string::npos) {
      parts.emplace_back(str.substr(begin, end));
      break;
    } else {
      parts.emplace_back(str.substr(begin, end - begin));
      begin = end + pattern.length();
    }
  }

  return parts;
}

std::string StringUtil::join(const std::vector<std::string>& list, const std::string& join) {
  std::string out;

  for (size_t i = 0; i < list.size(); ++i) {
    if (i > 0) {
      out += join;
    }

    out += list[i];
  }

  return out;
}

std::string StringUtil::join(const std::set<std::string>& list, const std::string& join) {
  std::string out;

  size_t i = 0;
  for (const auto& item : list) {
    if (++i > 1) {
      out += join;
    }

    out += item;
  }

  return out;
}

bool StringUtil::beginsWith(const std::string& str, const std::string& prefix) {
  if (str.length() < prefix.length()) {
    return false;
  }

  return str.compare(
      0,
      prefix.length(),
      prefix) == 0;
}

bool StringUtil::beginsWithIgnoreCase(const std::string& str, const std::string& prefix) {
  if (str.length() < prefix.length()) {
    return false;
  }

  return strncasecmp(str.data(),
                     prefix.data(),
                     prefix.size()) == 0;
}

bool StringUtil::endsWith(const std::string& str, const std::string& suffix) {
  if (str.length() < suffix.length()) {
    return false;
  }

  return str.compare(
      str.length() - suffix.length(),
      suffix.length(),
      suffix) == 0;
}

bool StringUtil::endsWithIgnoreCase(const std::string& str, const std::string& suffix) {
  if (str.length() < suffix.length()) {
    return false;
  }

  return strncasecmp(str.data() + str.length() - suffix.length(),
                     suffix.data(),
                     suffix.length()) == 0;
}

int StringUtil::compare(
    const char* s1,
    size_t s1_len,
    const char* s2,
    size_t s2_len) {
  for (; s1_len > 0 && s2_len > 0; s1++, s2++, --s1_len, --s2_len) {
    if (*s1 != *s2) {
      return (*(uint8_t *) s1 < *(uint8_t *) s2) ? -1 : 1;
    }
  }

  if (s1_len > 0) {
    return 1;
  }

  if (s2_len > 0) {
    return -1;
  }

  return 0;
}


bool StringUtil::isHexString(const std::string& str) {
  for (const auto& c : str) {
    if ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F')) {
      continue;
    }

    return false;
  }

  return true;
}

bool StringUtil::isAlphanumeric(const std::string& str) {
  for (const auto& c : str) {
    if (!isAlphanumeric(c)) {
      return false;
    }
  }

  return true;
}

bool StringUtil::isAlphanumeric(char chr) {
  bool is_alphanum =
      (chr >= '0' && chr <= '9') ||
      (chr >= 'a' && chr <= 'z') ||
      (chr >= 'A' && chr <= 'Z');

  return is_alphanum;
}

bool StringUtil::isShellSafe(const std::string& str) {
  for (const auto& c : str) {
    if (!isShellSafe(c)) {
      return false;
    }
  }

  return true;
}

bool StringUtil::isShellSafe(char chr) {
  bool is_safe =
      (chr >= '0' && chr <= '9') ||
      (chr >= 'a' && chr <= 'z') ||
      (chr >= 'A' && chr <= 'Z') ||
      (chr == '_') ||
      (chr == '-') ||
      (chr == '.');

  return is_safe;
}

bool StringUtil::isDigitString(const std::string& str) {
  return isDigitString(str.data(), str.data() + str.size());
}

bool StringUtil::isDigitString(const char* begin, const char* end) {
  for (auto cur = begin; cur < end; ++cur) {
    if (!isdigit(*cur)) {
      return false;
    }
  }

  return true;
}

bool StringUtil::isNumber(const std::string& str) {
  return isNumber(str.data(), str.data() + str.size());
}

bool StringUtil::isNumber(const char* begin, const char* end) {
  auto cur = begin;

  if (cur < end && *cur == '-') {
    ++cur;
  }

  for (; cur < end; ++cur) {
    if (!isdigit(*cur)) {
      return false;
    }
  }

  if (cur < end && (*cur == '.' || *cur == ',')) {
    ++cur;
  }

  for (; cur < end; ++cur) {
    if (!isdigit(*cur)) {
      return false;
    }
  }

  return true;
}

void StringUtil::toLower(std::string* str) {
  auto& str_ref = *str;

#if defined(XZERO_OS_WIN32)
  std::locale lc;
  for (size_t i = 0; i < str_ref.length(); ++i)
    str_ref[i] = std::tolower(str_ref[i], lc);
#else
  for (size_t i = 0; i < str_ref.length(); ++i)
    str_ref[i] = std::tolower(str_ref[i]);
#endif
}

std::string StringUtil::toLower(const std::string& str) {
  std::string copy = str;
  toLower(&copy);
  return copy;
}

void StringUtil::toUpper(std::string* str) {
  auto& str_ref = *str;

#if defined(XZERO_OS_WIN32)
  std::locale lc;
  for (size_t i = 0; i < str_ref.length(); ++i)
    str_ref[i] = std::toupper(str_ref[i], lc);
#else
  for (size_t i = 0; i < str_ref.length(); ++i)
    str_ref[i] = std::toupper(str_ref[i]);
#endif
}

std::string StringUtil::toUpper(const std::string& str) {
  std::string copy = str;
  toUpper(&copy);
  return copy;
}

size_t StringUtil::find(const std::string& str, char chr) {
  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] == chr) {
      return i;
    }
  }

  return -1;
}

size_t StringUtil::findLast(const std::string& str, char chr) {
  for (int i = str.length() - 1; i >= 0; --i) {
    if (str[i] == chr) {
      return i;
    }
  }

  return -1;
}

bool StringUtil::includes(const std::string& str, const std::string& subject) {
  return str.find(subject) != std::string::npos;
}

std::string StringUtil::hexPrint(
    const void* data,
    size_t size,
    bool sep /* = true */,
    bool reverse /* = fase */) {
  // FIXME: that's bad, as it's recreating the already existing buffer - just to hexprint it?
  Buffer buf((char*) data, size);
  return BufferUtil::hexPrint(&buf, sep, reverse);
}

std::wstring StringUtil::convertUTF8To16(const std::string& str) {
  std::wstring out;

  const char* cur = str.data();
  const char* end = cur + str.length();
  char32_t chr;
  while ((chr = UTF8::nextCodepoint(&cur, end)) > 0) {
    out += (wchar_t) chr;
  }

  return out;
}

std::string StringUtil::convertUTF16To8(const std::wstring& str) {
  std::string out;

  for (const auto& c : str) {
    UTF8::encodeCodepoint(c, &out);
  }

  return out;
}

std::string StringUtil::stripShell(const std::string& str) {
  std::string out;

  for (const auto& c : str) {
    if (isAlphanumeric(c) || c == '_' || c == '-' || c == '.') {
      out += c;
    }
  }

  return out;
}

std::string StringUtil::sanitizedStr(const std::string& str) {
  return sanitizedStr(str.data(), str.data() + str.size());
}

std::string StringUtil::sanitizedStr(const BufferRef& buffer) {
  return sanitizedStr(buffer.begin(), buffer.end());
}

std::string StringUtil::sanitizedStr(const char* begin, const char* end) {
  std::stringstream sstr;

  while (begin != end) {
    unsigned char ch = (unsigned char) *begin;
    ++begin;
#if defined(XZERO_OS_WIN32)
    std::locale lc;
    if (!std::isprint(ch, lc)) {
#else
    if (!std::isprint(ch)) {
#endif
      char buf[5];
      snprintf(buf, sizeof(buf), "\\x%02x", (unsigned) ch);
      sstr << buf;
    } else {
      switch (ch) {
        case '\t':
          sstr << "\\t";
          break;
        case '\r':
          sstr << "\\r";
          break;
        case '\n':
          sstr << "\\n";
          break;
        default:
          sstr << ch;
          break;
      }
    }
  }

  return sstr.str();
}

} // namespace xzero
