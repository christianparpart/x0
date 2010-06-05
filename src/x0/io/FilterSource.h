#ifndef sw_x0_io_FilterSource_h
#define sw_x0_io_FilterSource_h 1

#include <x0/io/Source.h>
#include <x0/io/BufferSource.h>
#include <x0/io/Filter.h>
#include <x0/io/SourceVisitor.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** Filter source.
 */
class X0_API FilterSource :
	public Source
{
public:
	explicit FilterSource(Filter& Filter) :
		buffer_(), source_(std::make_shared<BufferSource>("")), filter_(Filter), eof_(true), eof_touched_(false) {}

	FilterSource(const SourcePtr& source, Filter& Filter) :
		buffer_(), source_(source), filter_(Filter), eof_(false), eof_touched_(false) {}

	virtual BufferRef pull(Buffer& output)
	{
		std::size_t pos = output.size();

		buffer_.clear();

		if (!eof_)
			output.push_back(filter_(source_->pull(buffer_), false));
		else if (!eof_touched_)
		{
			eof_touched_ = true;
			output.push_back(filter_(source_->pull(buffer_), true));
		}

		return output.ref(pos);
	}

	virtual void accept(SourceVisitor& v)
	{
		v.visit(*this);
	}

protected:
	Buffer buffer_;
	SourcePtr source_;
	Filter& filter_;
	bool eof_;
	bool eof_touched_;
};

//@}

} // namespace x0

#endif
