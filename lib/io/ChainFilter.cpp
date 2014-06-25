// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/io/ChainFilter.h>

namespace x0 {

Buffer ChainFilter::process(const BufferRef& input)
{
    auto i = filters_.begin();
    auto e = filters_.end();

    if (i == e)
        return Buffer(input);

    Buffer result((*i++)->process(input));

    while (i != e)
        result = (*i++)->process(result.ref());

    return result;
}

} // namespace x0
