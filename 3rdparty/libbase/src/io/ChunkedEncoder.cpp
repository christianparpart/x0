// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/ChunkedEncoder.h>
#include <base/StackTrace.h>
#include <cassert>
#include <zlib.h>

#if 0  // !defined(NDEBUG)
#define TRACE(msg...) DEBUG("ChunkedEncoder: " msg)
#else
#define TRACE(msg...)
#endif

#define ERROR(msg...)                      \
  do {                                     \
    TRACE(msg);                            \
    StackTrace st;                         \
    TRACE("Stack Trace:\n%s", st.c_str()); \
  } while (0)

namespace base {

ChunkedEncoder::ChunkedEncoder() : finished_(false) {}

Buffer ChunkedEncoder::process(const BufferRef& input) {
#if 0
    if (input.empty())
        ERROR("proc: EOF");
    else
        TRACE("proc: inputLen=%ld", input.size());
#endif

  if (finished_) return Buffer();

  if (input.empty()) finished_ = true;

  Buffer output;

  if (input.size()) {
    char buf[12];
    int size = snprintf(buf, sizeof(buf), "%zx\r\n", input.size());

    output.push_back(buf, size);
    output.push_back(input);
    output.push_back("\r\n");
  } else
    output.push_back("0\r\n\r\n");

  //! \todo a filter might create multiple output-chunks, though, we could
  //improve process() to not return the output but append them directly.
  // virtual void process(const BufferRef& input, composite_source& output) = 0;

  return output;
}

}  // namespace base
