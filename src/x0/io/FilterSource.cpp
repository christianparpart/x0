#include <x0/io/FilterSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/Filter.h>
#include <x0/io/SourceVisitor.h>
#include <x0/StackTrace.h>
#include <x0/Defines.h>
#include <memory>

namespace x0 {

BufferRef FilterSource::pull(Buffer& output)
{
	if (eof_)
	{
		DEBUG("FilterSource: (eof=%d)", eof_);
		DEBUG("StackTrace:\n%s", StackTrace().c_str());
		return BufferRef();
	}

	std::size_t pos = output.size();

	buffer_.clear();
	BufferRef input = source_->pull(buffer_);

	if (input.empty())
		eof_ = true;

	Buffer filtered = filter_(input, eof_);
	output.push_back(filtered);
	DEBUG("FilterSource: #%ld -> #%ld (eof=%d)", input.size(), filtered.size(), eof_);
	if (eof_)
		DEBUG("[%s]", filtered.c_str());

	return output.ref(pos);
}

void FilterSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

} // namespace x0
