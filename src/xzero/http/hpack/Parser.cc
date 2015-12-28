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

Parser::Parser(size_t maxSize)
    : dynamicTable_(maxSize) {
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

size_t Parser::decodeInt(uint64_t* output, iterator pos, iterator end) {
  return 0; // TODO
}

} // namespace hpack
} // namespace http
} // namespace xzero
