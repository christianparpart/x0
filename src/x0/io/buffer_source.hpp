#ifndef sw_x0_io_buffer_source_hpp
#define sw_x0_io_buffer_source_hpp 1

#include <x0/io/buffer.hpp>
#include <x0/io/source.hpp>
#include <x0/io/source_visitor.hpp>
#include <memory>

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
	explicit buffer_source(const x0::buffer& data);
	explicit buffer_source(std::shared_ptr<x0::buffer> data);

	std::size_t size() const;
	bool empty() const;

	x0::buffer& buffer() const;
	x0::buffer *operator->() const;

	virtual buffer_ref pull(x0::buffer& result);
	virtual void accept(source_visitor& v);

public:
	void clear();
	std::size_t bytes_consumed() const;
	std::size_t bytes_available() const;

private:
	std::shared_ptr<x0::buffer> buffer_;
	std::size_t pos_;
};

//@}

// {{{ inlines
inline buffer_source::buffer_source(const x0::buffer& data) :
	buffer_(std::make_shared<x0::buffer>(data)), pos_(0)
{
}

inline buffer_source::buffer_source(std::shared_ptr<x0::buffer> data) :
	buffer_(data), pos_(0)
{
}

inline std::size_t buffer_source::size() const
{
	return buffer_->size();
}

inline bool buffer_source::empty() const
{
	return buffer_->empty();
}

inline x0::buffer& buffer_source::buffer() const
{
	return *buffer_;
}

inline x0::buffer *buffer_source::operator->() const
{
	return buffer_.get();
}

inline buffer_ref buffer_source::pull(x0::buffer& result)
{
	std::size_t result_pos = result.size();

	std::size_t first = pos_;
	pos_ = std::min(buffer_->size(), pos_ + x0::buffer::CHUNK_SIZE);

	result.push_back(buffer_->ref(first, pos_ - first));

	return result.ref(result_pos);
}

inline void buffer_source::accept(source_visitor& v)
{
	v.visit(*this);
}

inline void buffer_source::clear()
{
	pos_ = 0;
}

inline std::size_t buffer_source::bytes_consumed() const
{
	return pos_;
}

inline std::size_t buffer_source::bytes_available() const
{
	return buffer_->size() - pos_;
}
// }}}

} // namespace x0

#endif
