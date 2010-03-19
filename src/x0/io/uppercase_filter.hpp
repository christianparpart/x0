#ifndef sw_x0_io_uppercase_filter_hpp
#define sw_x0_io_uppercase_filter_hpp 1

#include <x0/io/filter.hpp>
#include <cctype>

namespace x0 {

//! \addtogroup io
//@{

/** simply transforms all letters into upper-case letters (test filter).
 */
class X0_API uppercase_filter :
	public filter
{
public:
	uppercase_filter();

	virtual buffer process(const buffer_ref& data, bool eof = false);

private:
	buffer work_;
};

// {{{ inline impl
inline uppercase_filter::uppercase_filter() :
	work_()
{
}

inline buffer uppercase_filter::process(const buffer_ref& data, bool eof)
{
	work_.clear();

	for (buffer_ref::iterator i = data.begin(), e = data.end(); i != e; ++i)
		work_.push_back(std::toupper(*i));

	return work_;
}
// }}}

//@}

} // namespace x0

#endif
