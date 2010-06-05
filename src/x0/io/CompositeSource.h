#ifndef sw_x0_io_CompositeSource_h
#define sw_x0_io_CompositeSource_h 1

#include <x0/io/Source.h>
#include <x0/Buffer.h>
#include <x0/io/SourceVisitor.h>

#include <vector>

namespace x0 {

//! \addtogroup io
//@{

/** composite source.
 *
 * This source represents a sequential set of sub sources.
 * \see source
 */
class X0_API CompositeSource :
	public Source
{
public:
	typedef SourcePtr value_type;
	typedef std::vector<value_type> vector_type;
	typedef vector_type::iterator iterator;
	typedef vector_type::const_iterator const_iterator;

private:
	vector_type sources_;
	std::size_t current_;

public:
	CompositeSource() :
		Source(),
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

	void push_back(SourcePtr s)
	{
		sources_.push_back(s);
	}

	SourcePtr operator[](std::size_t index) const
	{
		return sources_[index];
	}

public:
	virtual BufferRef pull(Buffer& output)
	{
		while (current_ < size())
		{
			if (BufferRef ref = sources_[current_]->pull(output))
				return ref;

			++current_;
		}

		return BufferRef();
	}

	virtual void accept(SourceVisitor& v)
	{
		v.visit(*this);
	}
};

//@}

} // namespace x0

#endif
