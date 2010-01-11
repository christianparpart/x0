#ifndef sw_x0_io_buffer_source_hpp
#define sw_x0_io_buffer_source_hpp 1

#include <x0/buffer.hpp>
#include <x0/source.hpp>
#include <x0/source_visitor.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** buffer source.
 *
 * \see source, sink
 */
class X0_API buffer_source :
	public source
{
public:
	buffer_source(const buffer& data) :
		buffer_(data), pos_(0)
	{
	}

	virtual buffer::view pull(buffer& data)
	{
		std::size_t first = pos_;

		pos_ = std::min(buffer_.size(), pos_ + x0::buffer::CHUNK_SIZE);

		return buffer_.sub(first, pos_ - first);
	}

	virtual void accept(source_visitor& v)
	{
		v.visit(*this);
	}

public:
	void clear()
	{
		pos_ = 0;
	}

	std::size_t bytes_consumed() const
	{
		return pos_;
	}

	std::size_t bytes_available() const
	{
		return buffer_.size() - pos_;
	}

private:
	x0::buffer buffer_;
	std::size_t pos_;
};

//@}

} // namespace x0

#endif
