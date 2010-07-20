#if !defined(sw_x0_io_FilterSink_h)
#define sw_x0_io_FilterSink_h (1)

#include <x0/io/Sink.h>
#include <x0/io/Filter.h>

namespace x0 {

class X0_API FilterSink :
	public Sink
{
private:
	Filter& filter_;
	SinkPtr sink_;

public:
	FilterSink(Filter& filter, const SinkPtr& sink) :
		filter_(filter), sink_(sink) {}

	virtual ~FilterSink() {}

	virtual ssize_t pump(Source& src)
	{
	}
};


} // namespace x0

#endif
