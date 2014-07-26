// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/CompositeSource.h>
#include <deque>

namespace base {

CompositeSource::~CompositeSource() { clear(); }

ssize_t CompositeSource::sendto(Sink& sink) {
  ssize_t result = 0;

  while (!empty()) {
    std::unique_ptr<Source>& front = sources_.front();
    ssize_t rv = front->sendto(sink);

    if (rv < 0) {  // error in source
      return result ? result : rv;
    } else if (rv == 0) {  // empty source
      sources_.pop_front();
    } else {
      result += rv;
    }
  }

  return result;
}

const char* CompositeSource::className() const { return "CompositeSource"; }

}  // namespace base
