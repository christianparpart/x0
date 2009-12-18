#ifndef sw_x0_io_filter_sink_hpp
#define sw_x0_io_filter_sink_hpp 1

namespace x0 {

class filter_sink :
	public sink
{
public:
	filter_sink(const filter& filter, const sink& sink) :
		filter_(filter), sink_(sink) {}

	virtual void push(const chunk& data)
	{
		sink_(filter_(data));
	}
};

} // namespace x0

#endif
