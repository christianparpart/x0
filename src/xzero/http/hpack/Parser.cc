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
#include <xzero/RuntimeError.h>

namespace xzero {
namespace http {
namespace hpack {

Parser::Parser(size_t maxSize, Emitter emitter)
    : dynamicTable_(maxSize),
      emitter_(emitter) {
}

bool Parser::parse(const BufferRef& headerBlock) {
  iterator i = iterator(headerBlock.begin());
  iterator e = iterator(headerBlock.end());

  while (i != e) {
    value_type octet = *i;

    if (octet & (1 << 7)) {
      // indexed header field
      i = indexedHeaderField(i, e);
    }
    else if (octet & (1 << 6)) {
      // literal header field with incremental indexing
      i = incrementalIndexedField(i, e);
    }
    else if (octet & (1 << 5)) {
      // dynamic table size update
      i = updateTableSize(i, e);
    }
    else if (octet & (1 << 4)) {
      // literal header field (never indexed)
      i = literalHeader(i, e);
    }
    else {
      // literal header field (without indexing)
      i = literalHeader(i, e);
    }
  }
  return i == e;
}

Parser::iterator Parser::indexedHeaderField(iterator i, iterator e) {
  return i;
}

Parser::iterator Parser::incrementalIndexedField(iterator i, iterator e) {
  return i;
}

Parser::iterator Parser::updateTableSize(iterator i, iterator e) {
  return i;
}

Parser::iterator Parser::literalHeader(iterator i, iterator e) {
  return i;
}

// 2^n
#define BIT(n) (1 << (n))

// LSB from 0 to n set, rest cleared
#define BITMASK(n) (BIT(n) - 1)

size_t Parser::decodeInt(uint8_t prefixBits, uint64_t* output,
                         iterator pos, iterator end) {
  assert(prefixBits <= 8);
  assert(pos != end);

  *output = *pos;

  if (prefixBits < 8)
    *output &= BITMASK(prefixBits);

  if (*output < BITMASK(prefixBits))
    return 1;

  size_t nbytes = 1;
  while (pos != end) {
    uint8_t part = *pos & BITMASK(7);
    *output += part;
    pos++;
    nbytes++;

    if ((part & BIT(7)) == 0) {
      return nbytes;
    }
  }

  RAISE(RuntimeError, "Need more data");
}

} // namespace hpack
} // namespace http
} // namespace xzero
