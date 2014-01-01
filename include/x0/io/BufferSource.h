/* <BufferSource.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_BufferSource_hpp
#define sw_x0_io_BufferSource_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Source.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** buffer source.
 *
 * \see Buffer, Source, Sink
 */
class X0_API BufferSource :
	public Source
{
public:
	template<typename PodType, std::size_t N> explicit BufferSource(PodType (&value)[N]);
	explicit BufferSource(const Buffer& data);
	explicit BufferSource(Buffer&& data);
	~BufferSource();

	std::size_t size() const;
	bool empty() const;

	virtual ssize_t sendto(Sink& sink);

	virtual const char* className() const;

private:
	Buffer buffer_;
	std::size_t pos_;
};

//@}

// {{{ inlines
template<typename PodType, std::size_t N>
inline BufferSource::BufferSource(PodType (&value)[N]) :
	buffer_(), pos_(0)
{
	buffer_.push_back(value, N - 1);
}

inline BufferSource::BufferSource(const Buffer& data) :
	buffer_(data), pos_(0)
{
}

inline BufferSource::BufferSource(Buffer&& data) :
	buffer_(std::move(data)), pos_(0)
{
}

inline BufferSource::~BufferSource()
{
}

inline std::size_t BufferSource::size() const
{
	return buffer_.size() - pos_;
}

inline bool BufferSource::empty() const
{
	return size() == 0;
}
// }}}

} // namespace x0

#endif
