#ifndef sw_x0_io_buffer_sink_hpp
#define sw_x0_io_buffer_sink_hpp 1

#include <x0/buffer.hpp>
#include <x0/sink.hpp>

namespace x0 {

//! \addtogroup io
//@{

/** sink, storing incoming data into a buffer.
 *
 * \see sink, source
 */
class X0_API buffer_sink :
	public sink
{
public:
	buffer_sink() :
		buffer_()
	{
	}

	virtual x0::buffer::view push(const x0::buffer::view& data)
	{
		// we always push the whole data into our buffer
		buffer_.push_back(data);
		return data;
	}

public:
	void clear()
	{
		buffer_.clear();
	}

	const x0::buffer& buffer() const
	{
		return buffer_;
	}

	std::size_t size() const
	{
		return buffer_.size();
	}

	bool empty() const
	{
		return buffer_.empty();
	}

private:
	x0::buffer buffer_;
};

//@}

} // namespace x0

#endif
