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
		buffer_(), source_(std::make_shared<BufferSource>("")), filter_(Filter), eof_(false) {}

	FilterSource(const SourcePtr& source, Filter& Filter) :
		buffer_(), source_(source), filter_(Filter), eof_(false) {}

	virtual BufferRef pull(Buffer& output);
	virtual bool eof() const;
	virtual void accept(SourceVisitor& v);

protected:
	Buffer buffer_;
	SourcePtr source_;
	Filter& filter_;
	bool eof_;
};

//@}

} // namespace x0

#endif
