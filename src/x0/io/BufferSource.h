#ifndef sw_x0_io_BufferSource_hpp
#define sw_x0_io_BufferSource_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Source.h>
#include <x0/io/SourceVisitor.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** buffer source.
 *
 * \see buffer, source, sink
 */
class X0_API BufferSource :
	public Source
{
public:
	template<typename PodType, std::size_t N> explicit BufferSource(PodType (&value)[N]);
	explicit BufferSource(const x0::BufferRef& data);
	explicit BufferSource(const Buffer& data);
	explicit BufferSource(const Buffer&& data);

	std::size_t size() const;
	bool empty() const;

	const Buffer& buffer() const;
	const Buffer *operator->() const;

	virtual BufferRef pull(Buffer& result);
	virtual void accept(SourceVisitor& v);

public:
	void clear();
	std::size_t bytes_consumed() const;
	std::size_t bytes_available() const;

private:
	Buffer buffer_;
	std::size_t pos_;
};

//@}

// {{{ inlines
template<typename PodType, std::size_t N>
inline BufferSource::BufferSource(PodType (&value)[N]) :
	buffer_(value, N - 1), pos_(0)
{
}

inline BufferSource::BufferSource(const x0::BufferRef& data) :
	buffer_(data), pos_(0)
{
}

inline BufferSource::BufferSource(const Buffer& data) :
	buffer_(data), pos_(0)
{
}

inline BufferSource::BufferSource(const Buffer&& data) :
	buffer_(data), pos_(0)
{
}

inline std::size_t BufferSource::size() const
{
	return buffer_.size();
}

inline bool BufferSource::empty() const
{
	return buffer_.empty();
}

inline const Buffer& BufferSource::buffer() const
{
	return buffer_;
}

inline const Buffer *BufferSource::operator->() const
{
	return &buffer_;
}

inline BufferRef BufferSource::pull(Buffer& result)
{
	std::size_t result_pos = result.size();

	std::size_t first = pos_;
	pos_ = std::min(buffer_.size(), pos_ + Buffer::CHUNK_SIZE);

	result.push_back(buffer_.ref(first, pos_ - first));

	return result.ref(result_pos);
}

inline void BufferSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

inline void BufferSource::clear()
{
	pos_ = 0;
}

inline std::size_t BufferSource::bytes_consumed() const
{
	return pos_;
}

inline std::size_t BufferSource::bytes_available() const
{
	return buffer_.size() - pos_;
}
// }}}

} // namespace x0

#endif
