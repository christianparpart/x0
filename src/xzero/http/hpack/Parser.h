// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {

class HeaderField;
class HeaderFieldList;

namespace hpack {

class Parser {
 public:
  bool parse(const BufferRef& headerBlock);

  static size_t decodeInt(uint64_t* output,
                          const unsigned char* pos,
                          const unsigned char* end);

 private:
  DynamicTable dynamicTable_;
};

} // namespace hpack
} // namespace http
} // namespace xzero
