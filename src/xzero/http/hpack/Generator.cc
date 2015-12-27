// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/Generator.h>
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
}

void Generator::clear() {
  headerBlock_.clear();
}

void Generator::reset() {
  dynamicTable_.clear();
  headerBlock_.clear();

  headerBlock_.push_back(0x00); // TODO: instr. clear dyntable
}

void Generator::generateHeaders(const HeaderFieldList& fields) {
  for (const HeaderField& field: fields) {
    generateHeader(field);
  }
}

void Generator::generateHeader(const HeaderField& field) {
  // TODO
}

size_t Generator::encodeInt(uint64_t value,
                            unsigned prefixBits,
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

} // namespace hpack
} // namespace http
} // namespace xzero
