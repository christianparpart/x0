// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/Generator.h>
#include <xzero/http/hpack/StaticTable.h>
#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/http/hpack/Huffman.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HeaderField.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace hpack {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.hpack.Generator", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

Generator::Generator(size_t maxSize)
    : dynamicTable_(maxSize),
      headerBlock_() {
  headerBlock_.reserve(maxSize);
}

void Generator::setMaxSize(size_t maxSize) {
  dynamicTable_.setMaxSize(maxSize);
  headerBlock_.reserve(maxSize);
}

void Generator::clear() {
  headerBlock_.clear();
}

void Generator::reset() {
  dynamicTable_.clear();
  headerBlock_.clear();

  // set header table size to 0 (for full eviction)
  encodeInt(1, 5, (size_t) 0);

  // now set it back to some meaningful value
  encodeInt(1, 5, dynamicTable_.maxSize());
}

void Generator::generateHeaders(const HeaderFieldList& fields) {
  for (const HeaderField& field: fields) {
    generateHeader(field);
  }
}

void Generator::generateHeader(const HeaderField& field) {
  // search in static table
  bool nameValueMatch;
  size_t index = StaticTable::find(field, &nameValueMatch);
  if (index != StaticTable::npos) {
    if (nameValueMatch) {
      // (6.1) indexed header field
      encodeInt(1, 7, index);
    } else if (isIndexable(field)) {
      // (6.2.1) indexed name, literal value, indexable
      dynamicTable_.add(field);
      encodeInt(1, 6, index);
      encodeString(field.value());
    } else {
      // (6.2.2) indexed name, literal value, non-indexable
      encodeInt(0, 4, index);
      encodeString(field.value());
    }
  }

  // search in dynamic table
  index = dynamicTable_.find(field, &nameValueMatch);
  if (index != DynamicTable::npos) {
    index += StaticTable::length();
    if (nameValueMatch) {
      // indexed header field (6.1)
      encodeInt(1, 7, index);
    } else if (isIndexable(field)) {
      // (6.2.1) indexed name, literal value, indexable
      dynamicTable_.add(field);
      encodeInt(1, 6, index);
      encodeString(field.value());
    } else {
      // (6.2.2) indexed name, literal value, non-indexable
      encodeInt(0, 4, index);
      encodeString(field.value());
    }
  }

  // literal name, literal value
  if (isIndexable(field)) {
    // with future-indexing
    dynamicTable_.add(field);
    headerBlock_.push_back(1 << 6); // 0100_0000
    encodeString(field.name());
    encodeString(field.value());
  } else {
    // without future-indexing
    headerBlock_.push_back(0);
    encodeString(field.name());
    encodeString(field.value());
  }
}

void Generator::encodeString(const std::string& value, bool compressed) {
  // (5.2) String Literal Representation

  if (compressed) {
    std::string smaller = Huffman::compress(value);
    encodeInt(1, 7, smaller.size());
    headerBlock_.push_back(smaller);
  } else {
    // Huffman encoding disabled
    encodeInt(0, 7, value.size());
    headerBlock_.push_back(value);
  }
}

void Generator::encodeInt(uint8_t suffix, uint8_t prefixBits, uint64_t value) {
  unsigned char* output = (unsigned char*) headerBlock_.end();

  headerBlock_.reserve(headerBlock_.size() + 4);
  size_t n = encodeInt(value, prefixBits, output); // TODO: add suffix here instead
  *output |= suffix << prefixBits;
  headerBlock_.resize(headerBlock_.size() + n);
}

size_t Generator::encodeInt(uint64_t value,
                            uint8_t prefixBits,
                            unsigned char* output) {
  assert(prefixBits >= 1 && prefixBits <= 8);

  const unsigned maxValue = (1 << prefixBits) - 1;

  if (value < maxValue) {
    *output = value;
    return 1;
  } else {
    *output++ = static_cast<char>(maxValue);
    value -= maxValue;
    size_t n = 2;

    while (value >= 128) {
      const unsigned char byte = (1 << 7) | (value & 0x7f);
      *output++ = byte;
      value /= 128;
      n++;
    }

    *output = static_cast<unsigned char>(value);
    return n;
  }
}

bool Generator::isIndexable(const HeaderField& field) const {
  return !field.isSecure();
}

} // namespace hpack
} // namespace http
} // namespace xzero
