// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/io/NullSource.h>

namespace x0 {

ssize_t NullSource::sendto(Sink& sink) { return 0; }

ssize_t NullSource::size() const { return 0; }

const char* NullSource::className() const { return "NullSource"; }

}  // namespace x0
