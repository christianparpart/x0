/* <x0/Tokenizer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/StringTokenizer.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <string>

namespace x0 {

template class Tokenizer<std::string>;
template class Tokenizer<BufferRef>;

} // namespace x0
