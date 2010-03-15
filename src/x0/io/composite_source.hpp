#ifndef sw_x0_io_composite_source_hpp
#define sw_x0_io_composite_source_hpp 1

#include <x0/io/source.hpp>
#include <x0/buffer.hpp>
#include <x0/io/source_visitor.hpp>

#include <vector>

namespace x0 {

//! \addtogroup io
//@{

/** composite source.
 *
 * This source represents a sequential set of sub sources.
 * \see source
 */
class X0_API composite_source :
	public source
{
public:
	typedef source_ptr value_type;
	typedef std::vector<value_type> vector_type;
	typedef vector_type::iterator iterator;
	typedef vector_type::const_iterator const_iterator;

private:
	vector_type sources_;
	std::size_t current_;

public:
	composite_source() :
		source(),
		sources_(),
		current_(0)
	{
	}

	std::size_t size() const
	{
		return sources_.size();
	}

	bool empty() const
	{
		return sources_.empty();
	}

	void push_back(source_ptr s)
	{
		sources_.push_back(s);
	}

	source_ptr operator[](std::size_t index) const
	{
		return sources_[index];
	}

public:
	virtual buffer_ref pull(buffer& output)
	{
		while (current_ < size())
		{
			if (buffer_ref ref = sources_[current_]->pull(output))
				return ref;

			++current_;
		}

		return buffer_ref();
	}

	virtual void accept(source_visitor& v)
	{
		v.visit(*this);
	}
};

//@}

} // namespace x0

#endif
