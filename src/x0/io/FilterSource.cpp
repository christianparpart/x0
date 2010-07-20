#include <x0/io/FilterSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/Filter.h>
#include <x0/io/SourceVisitor.h>
#include <x0/Defines.h>
#include <memory>

#if !defined(NDEBUG)
#	include <x0/StackTrace.h>
#endif

namespace x0 {

BufferRef FilterSource::pull(Buffer& output)
{
	if (eof_)
	{
		DEBUG("FilterSource: WARNING: pull() invoked *after* EOF has been reached.");
		DEBUG("StackTrace:\n%s", StackTrace().c_str());
		return BufferRef();
	}

	std::size_t pos = output.size();

	buffer_.clear();
	BufferRef input = source_->pull(buffer_);

	if (source_->eof())
		eof_ = true;

	Buffer filtered = filter_(input, eof_);
	output.push_back(filtered);

	//DEBUG("FilterSource: #%ld -> #%ld (eof=%d)", input.size(), filtered.size(), eof_);

#if 0 //!defined(NDEBUG)
	if (eof_)
		DEBUG("eof[%s]", filtered.c_str());
#endif

	return output.ref(pos);
}

bool FilterSource::eof() const
{
	return eof_;
}

void FilterSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

} // namespace x0
