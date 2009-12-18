#ifndef sw_x0_io_filter_source_hpp
#define sw_x0_io_filter_source_hpp 1

namespace x0 {

class filter_source :
	public source
{
public:
	explicit filter_source(const source& source, const filter& filter) :
		source_(source), filter_(filter) {}

	virtual chunk pull()
	{
		return filter_(source_());
	}
};

} // namespace x0

#endif
