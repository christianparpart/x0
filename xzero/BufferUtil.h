// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _STX_BASE_BUFFERUTIL_H_
#define _STX_BASE_BUFFERUTIL_H_

#include <stdlib.h>
#include <stdint.h>
#include <xzero/Buffer.h>
#include <functional>

namespace xzero {

class BufferUtil {
public:

  /**
   * Remove trailing bytes from the pointed to buffer
   *
   * @param str the string to remove trailing slashes from
   */
  static void stripTrailingBytes(Buffer* buf, unsigned char byte);

  /**
   * Remove trailing slashes from the pointed to buffer
   *
   * @param str the string to remove trailing slashes from
   */
  static void stripTrailingSlashes(Buffer* buf);

  /**
   * Print the contents of the pointed to memory as a series of hexadecimal
   * bytes (hexdump):
   *
   * Example:
   *   StringUtil::hexPrint("\x17\x23\x42\x01", 4);
   *   // returns "17 23 42 01"
   *
   * @param data the data to print
   * @param size the size of the data in bytes
   * @param separate_bytes if true, insert a whitespace character between bytes
   * @param reverse_byte_order if true, print the data from last to first byte
   * @return the data formatted as a human readable hex string
   */
  static std::string hexPrint(
      Buffer* buf,
      bool separate_bytes = true,
      bool reverse_byte_order = false);

  static std::string binPrint(const BufferRef& data, bool spacing = false);

  static bool beginsWith(const BufferRef& data, const BufferRef& prefix);
  static bool beginsWithIgnoreCase(const BufferRef& str, const BufferRef& prefix);
  static bool endsWith(const BufferRef& data, const BufferRef& suffix);
  static bool endsWithIgnoreCase(const BufferRef& data, const BufferRef& suffix);

  static std::function<void(const uint8_t*, size_t)> writer(Buffer* output);
  static std::function<void(const uint8_t*, size_t)> writer(std::vector<uint8_t>* output);
};

} // namespace xzero

#endif
