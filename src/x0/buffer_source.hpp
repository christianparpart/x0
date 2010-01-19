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
 * \see buffer, source, sink
 */
class X0_API buffer_source :
	public source
{
public:
	buffer_source(const buffer& data) :
		buffer_(data), pos_(0)
	{
	}

	std::size_t size() const
	{
		return buffer_.size();
	}

	const x0::buffer& buffer() const
	{
		return buffer_;
	}

	bool empty() const
	{
		return buffer_.empty();
	}

	virtual buffer_ref pull(x0::buffer& result)
	{
		std::size_t result_pos = result.size();

		std::size_t first = pos_;
		pos_ = std::min(buffer_.size(), pos_ + x0::buffer::CHUNK_SIZE);

		result.push_back(buffer_.ref(first, pos_ - first));

		return result.ref(result_pos);
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
