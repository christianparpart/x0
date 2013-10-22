/* <BufferRefSource.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_io_BufferRefSource_hpp
#define sw_x0_io_BufferRefSource_hpp 1

#include <x0/Buffer.h>
#include <x0/io/Source.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** BufferRef source.
 *
 * \see BufferRef, Source, Sink
 */
class X0_API BufferRefSource :
	public Source
{
public:
	explicit BufferRefSource(const Buffer& data);
	explicit BufferRefSource(const BufferRef& data);
	explicit BufferRefSource(BufferRef&& data);
	~BufferRefSource();

	std::size_t size() const;
	bool empty() const;

	virtual ssize_t sendto(Sink& sink);
	virtual const char* className() const;

private:
	BufferRef buffer_;
	std::size_t pos_;
};

//@}

// {{{ inlines
inline BufferRefSource::BufferRefSource(const Buffer& data) :
	buffer_(data.ref()), pos_(0)
{
}

inline BufferRefSource::BufferRefSource(const BufferRef& data) :
	buffer_(data), pos_(0)
{
}

inline BufferRefSource::BufferRefSource(BufferRef&& data) :
	buffer_(std::move(data)), pos_(0)
{
}

inline BufferRefSource::~BufferRefSource()
{
}

inline std::size_t BufferRefSource::size() const
{
	return buffer_.size() - pos_;
}

inline bool BufferRefSource::empty() const
{
	return size() == 0;
}
// }}}

} // namespace x0

#endif
