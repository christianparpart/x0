// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/Parser.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HeaderField.h>

namespace xzero {
namespace http {
namespace hpack {

bool Parser::parse(const BufferRef& headerBlock) {
  const uint8_t* i = (const uint8_t*) headerBlock.begin();
  const uint8_t* e = (const uint8_t*) headerBlock.end();

  while (i != e) {
    uint8_t octet = *i;
    bool indexed = false;

    if (octet & (1 << 7)) {
      // indexed header field
      indexed = true;
    }
    else if (octet & (1 << 6)) {
      // literal header field with incremental indexing
    }
    else if (octet & (1 << 5)) {
      // dynamic table size update
    }
    else if (octet & (1 << 4)) {
      // literal header field (never indexed)
    }
    else {
      // literal header field (without indexing)
    }
  }
  return false;
}

size_t Parser::decodeInt(uint64_t* output,
                         const unsigned char* pos,
                         const unsigned char* end) {
  return 0; // TODO
}

} // namespace hpack
} // namespace http
} // namespace xzero
