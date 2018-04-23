// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/BufferUtil.h>
#include <xzero/inspect.h>
#include <xzero/defines.h>

#ifdef XZERO_OS_WIN32
#include <string.h>
#else
#include <strings.h>
#endif

namespace xzero {

void BufferUtil::stripTrailingBytes(Buffer* buf, unsigned char byte) {
  auto begin = (const unsigned char*) buf->data();
  auto cur = begin + buf->size();

  while (cur > begin && *(cur - 1) == byte) {
    cur--;
  }

  buf->truncate(cur - begin);
}

void BufferUtil::stripTrailingSlashes(Buffer* buf) {
  stripTrailingBytes(buf, '/');
}

std::string BufferUtil::hexPrint(
    Buffer* buf,
    bool sep /* = true */,
    bool reverse /* = fase */) {
  static const char hexTable[] = "0123456789abcdef";
  auto data = (const unsigned char*) buf->data();
  int size = static_cast<int>(buf->size());
  std::string str;

  if (!reverse) {
    for (int i = 0; i < size; ++i) {
      if (sep && i > 0) { str += " "; }
      auto byte = data[i];
      str += hexTable[(byte & 0xf0) >> 4];
      str += hexTable[byte & 0x0f];
    }
  } else {
    for (int i = size - 1; i >= 0; --i) {
      if (sep && i < size - 1) { str += " "; }
      auto byte = data[i];
      str += hexTable[(byte & 0xf0) >> 4];
      str += hexTable[byte & 0x0f];
    }
  }

  return str;
}

bool BufferUtil::beginsWith(const BufferRef& data, const BufferRef& prefix) {
  if (data.size() < prefix.size())
    return false;

  return data.ref(0, prefix.size()) == prefix;
}

bool BufferUtil::beginsWithIgnoreCase(const BufferRef& str, const BufferRef& prefix) {
  if (str.size() < prefix.size()) {
    return false;
  }
#if defined(_WIN32) || defined(_WIN64)
  return _strnicmp(str.data(),
                   prefix.data(),
                   prefix.size()) == 0;
#else
  return strncasecmp(str.data(),
                     prefix.data(),
                     prefix.size()) == 0;
#endif
}

bool BufferUtil::endsWith(const BufferRef& data, const BufferRef& suffix) {
  if (data.size() < suffix.size())
    return false;

  return data.ref(data.size() - suffix.size()) == suffix;
}

bool BufferUtil::endsWithIgnoreCase(const BufferRef& str, const BufferRef& suffix) {
  if (str.size() < suffix.size()) {
    return false;
  }
#if defined(_WIN32) || defined(_WIN64)
  return _strnicmp(str.data() + str.size() - suffix.size(),
                   suffix.data(),
                   suffix.size()) == 0;
#else
  return strncasecmp(str.data() + str.size() - suffix.size(),
                     suffix.data(),
                     suffix.size()) == 0;
#endif
}

std::string BufferUtil::binPrint(const BufferRef& data, bool spacing) {
  std::string s;

  for (size_t i = 0; i < data.size(); ++i) {
    if (spacing && i != 0)
      s += ' ';

    uint8_t byte = data[i];
    for (size_t k = 0; k < 8; ++k) {
      if (byte & (1 << (7 - k))) {
        s += '1';
      } else {
        s += '0';
      }
    }
  }

  return s;
}

std::function<void(const uint8_t*, size_t)> BufferUtil::writer(Buffer* output) {
  return [output](const uint8_t* data, size_t len) {
    output->push_back(data, len);
  };
}

std::function<void(const uint8_t*, size_t)> BufferUtil::writer(std::vector<uint8_t>* output) {
  return [output](const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      output->push_back(data[i]);
    }
  };
}

} // namespace xzero
