/* <x0/BufferSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/BufferSource.h>
#include <x0/io/SourceVisitor.h>

namespace x0 {

BufferRef BufferSource::pull(Buffer& result)
{
	std::size_t result_pos = result.size();

	std::size_t first = pos_;
	pos_ = std::min(buffer_.size(), pos_ + Buffer::CHUNK_SIZE);

	result.push_back(buffer_.ref(first, pos_ - first));

	return result.ref(result_pos);
}

bool BufferSource::eof() const
{
	return pos_ == buffer_.size();
}

void BufferSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

} // namespace x0
