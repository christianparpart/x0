// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpRangeDef.h>

// XXX http://tools.ietf.org/html/draft-fielding-http-p5-range-00

namespace xzero::http {

bool HttpRangeDef::parse(const BufferRef& value) {
  // ranges-specifier = byte-ranges-specifier
  // byte-ranges-specifier = bytes-unit "=" byte-range-set

  size_t i = value.find('=');
  if (i == BufferRef::npos)
    return false;

  unitName_ = value.ref(0, i);
  if (unitName_ != "bytes")
    return false;

  BufferRef ranges = value.ref(i + 1);
  i = 0;
  while (i < ranges.size()) {
    const size_t e = ranges.find(',', i);

    BufferRef range = ranges.ref(i, e);
    if (!parseByteRangeDef(range))
      return false;

    if (e == BufferRef::npos) {
      break;
    }
    i += e + 1;
  }

  return true;
}

bool HttpRangeDef::parseByteRangeDef(const char* begin, const char* end) {
  // byte-range-set  = 1#( byte-range-spec | suffix-byte-range-spec )
  // byte-range-spec = first-byte-pos "-" [last-byte-pos]
  // first-byte-pos  = 1*DIGIT
  // last-byte-pos   = 1*DIGIT
  // suffix-byte-range-spec = "-" suffix-length
  // suffix-length = 1*DIGIT

  const char* i = begin;
  const char* e = end;

  if (i == e)
    return false;

  auto isDigit = [](char p) -> bool { return p >= '0' && p <= '9'; };

  // parse first element
  char* eptr = const_cast<char*>(i);
  size_t a = isDigit(*i) ? strtoul(i, &eptr, 10) : npos;

  i = eptr;

  if (*i != '-') {
    // printf("parse error: %s (%s)\n", i, spec.c_str());
    return false;
  }

  ++i;

  if (i == e) {
    ranges_.emplace_back(a, npos);
    return true;
  }

  // parse second element
  size_t b = strtoul(i, &eptr, 10);

  i = eptr;

  if (i != e)  // garbage at the end
    return false;

  ranges_.push_back(std::make_pair(a, b));

  return true;
}

} // namespace xzero::http
