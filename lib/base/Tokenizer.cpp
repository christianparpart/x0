/* <x0/Tokenizer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Tokenizer.h>
#include <x0/Buffer.h>
#include <string>

namespace x0 {

template class Tokenizer<BufferRef, Buffer>;
template class Tokenizer<BufferRef, BufferRef>;

} // namespace x0
