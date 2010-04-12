#ifndef sw_x0_io_chain_filter_hpp
#define sw_x0_io_chain_filter_hpp 1

#include <x0/io/filter.hpp>
#include <deque>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** chaining filter API, supporting sub filters to be chained together.
 */
class X0_API chain_filter :
	public filter
{
public:
	virtual buffer process(const buffer_ref& input, bool eof = false);

public:
	void push_front(filter_ptr f);
	void push_back(filter_ptr f);

	std::size_t size() const;
	bool empty() const;

	const filter_ptr& operator[](std::size_t index) const;

private:
	std::deque<filter_ptr> filters_;
};

//{{{ inlines impl
inline void chain_filter::push_front(filter_ptr f)
{
	filters_.push_front(f);
}

inline void chain_filter::push_back(filter_ptr f)
{
	filters_.push_back(f);
}

inline std::size_t chain_filter::size() const
{
	return filters_.size();
}

inline bool chain_filter::empty() const
{
	return filters_.empty();
}

inline const filter_ptr& chain_filter::operator[](std::size_t index) const
{
	return filters_[index];
}
//}}}

//@}

} // namespace x0

#endif
