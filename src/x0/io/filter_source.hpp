#ifndef sw_x0_io_filter_source_hpp
#define sw_x0_io_filter_source_hpp 1

#include <x0/io/source.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/io/filter.hpp>
#include <x0/io/source_visitor.hpp>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** filter source.
 */
class X0_API filter_source :
	public source
{
public:
	explicit filter_source(filter& filter) :
		buffer_(), source_(std::make_shared<buffer_source>("")), filter_(filter), eof_(true), eof_touched_(false) {}

	filter_source(const source_ptr& source, filter& filter) :
		buffer_(), source_(source), filter_(filter), eof_(false), eof_touched_(false) {}

	virtual buffer_ref pull(buffer& output)
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

	virtual void accept(source_visitor& v)
	{
		v.visit(*this);
	}

protected:
	buffer buffer_;
	source_ptr source_;
	filter& filter_;
	bool eof_;
	bool eof_touched_;
};

//@}

} // namespace x0

#endif
